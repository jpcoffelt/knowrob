/*
 * Copyright (c) 2022, Daniel Beßler
 * All rights reserved.
 *
 * This file is part of KnowRob, please consult
 * https://github.com/knowrob/knowrob for license details.
 */

#ifndef KNOWROB_PROLOG_REASONER_H_
#define KNOWROB_PROLOG_REASONER_H_

// STD
#include <string>
#include <list>
#include <filesystem>
#include <map>
#include <memory>
// gtest
#include <gtest/gtest.h>
// KnowRob
#include <knowrob/ThreadPool.h>
#include <knowrob/terms.h>
#include <knowrob/queries.h>
#include <knowrob/LogicProgramReasoner.h>
#include <knowrob/prolog/PrologQuery.h>

namespace knowrob {
	/**
	 * A pool of threads with attached Prolog engines.
	 * Prolog threads have their own stacks and only share the Prolog heap:
	 * predicates, records, flags and other global non-backtrackable data.
	 */
	class PrologThreadPool : public ThreadPool {
	public:
		/**
		 * @maxNumThreads maximum number of worker threads.
		 */
		explicit PrologThreadPool(uint32_t maxNumThreads=0);

	protected:
		// Override ThreadPool
		bool initializeWorker() override;
		
		// Override ThreadPool
		void finalizeWorker() override;
	};
	
	class PrologDataFile : public DataFile {
	public:
		explicit PrologDataFile(const std::string &path);
	};

	/**
	 * A Prolog reasoner that answers queries using SWI Prolog.
	 */
	class PrologReasoner : public LogicProgramReasoner {
	public:
		/**
		 * @param reasonerID a knowledge base identifier of this reasoner.
		 */
		explicit PrologReasoner(std::string reasonerID);
		
		~PrologReasoner() override;

		PrologReasoner(const PrologReasoner&) = delete;

		/**
		 * Consults a Prolog file, i.e. loads facts and rules and executed
		 * directives in the file.
		 * May throw an exception if there is no valid Prolog file at the given path.
		 * @prologFile the local path to the file.
         * @return true on success
		 */
		virtual bool consult(const std::filesystem::path &prologFile);
		
		/**
		 * Evaluates a query and returns one solution if any.
		 * @return the first solution found, or QueryResultStream::eos() if none.
		 */
		std::shared_ptr<QueryResult> oneSolution(const std::shared_ptr<Query> &goal);
		
		/**
		 * Evaluates a query and returns all solutions.
		 * @return list of solutions.
		 */
		std::list<std::shared_ptr<QueryResult>> allSolutions(const std::shared_ptr<Query> &goal);
		
		/**
		 * Parse a string into a term.
		 */
		std::shared_ptr<Term> readTerm(const std::string &queryString);

		/**
		 * Run unittests associated to the given target name.
		 * The target name can be the name of a loaded testcase,
		 * or the path to a "*.pl", "*.plt" file, or the path to
		 * a directory containing such files.
		 * @param target a plunit target
		 * @return true on success
		 */
		std::list<TermPtr> runTests(const std::string &target);
		
		/**
		 * Transforms each input query.
		 * The intended purpose is that subclasses of this reasoner can wrap a goal in a
		 * reasoner specific higher-order predicate, e.g. `my_reasoner_call/1`.
		 */
		virtual std::shared_ptr<Query> transformQuery(const std::shared_ptr<Query> &q) { return q; }

		/**
		 * Resolve the path to a Prolog file.
		 * The function attempts to resolve project-relative paths.
		 * @param filename a name or path.
		 * @return a path where the file might be stored
		 */
		static std::filesystem::path getPrologPath(const std::filesystem::path &filename);

		// Override LogicProgramReasoner
		bool assertFact(const std::shared_ptr<Predicate> &predicate) override;

		// Override IReasoner
		bool loadConfiguration(const ReasonerConfiguration &cfg) override;

		// Override IReasoner
		bool isCurrentPredicate(const PredicateIndicator &predicate) override;

		// Override IReasoner
		void startQuery(uint32_t queryID,
			const std::shared_ptr<QueryResultStream::Channel> &outputStream,
			const std::shared_ptr<Query> &goal) override;
		
		// Override IReasoner
		void finishQuery(uint32_t queryID, bool isImmediateStopRequested) override;
		
		// Override IReasoner
		void pushSubstitution(uint32_t queryID, const SubstitutionPtr &bindings) override;

	protected:
		const std::string reasonerID_;

		/** a query request for a runner */
		struct Request {
			uint32_t queryID;
			const char *queryModule;
			std::shared_ptr<Query> goal;
			explicit Request(const std::shared_ptr<Query> &goal, const char *queryModule=nullptr, uint32_t queryID=0)
			: queryID(queryID), queryModule(queryModule), goal(goal) {};
		};

		/** A runner that evaluates a Prolog query.  */
		class Runner : public ThreadPool::Runner {
		public:
			Runner(PrologReasoner *reasoner,
				   Request request,
				   const std::shared_ptr<QueryResultStream::Channel> &outputStream,
				   bool sendEOS=false,
				   const SubstitutionPtr &bindings=QueryResultStream::eos());

			// Override Runner
			void run() override;

		protected:
			PrologReasoner *reasoner_;
			Request request_;
			SubstitutionPtr bindings_;
			std::shared_ptr<QueryResultStream::Channel> outputStream_;
			bool sendEOS_;

			std::list<std::shared_ptr<PrologReasoner::Runner>>::iterator requestIterator;
			friend class PrologReasoner;
		};
		
		struct ActiveQuery {
			std::shared_ptr<QueryResultStream::Channel> outputStream;
			std::shared_ptr<Query> goal;
			std::atomic<bool> hasReceivedAllInput;
			std::list<std::shared_ptr<PrologReasoner::Runner>> runner;
			std::mutex mutex;
		};
		using ActiveQueryMap = std::map<uint32_t, PrologReasoner::ActiveQuery*>;
		
		ActiveQueryMap activeQueries_;
		std::mutex request_mutex_;
		
		void finishRunner(uint32_t queryID, PrologReasoner::Runner *runner);
		
		bool consult(const std::shared_ptr<DataFile> &dataFile);
		bool consult(const std::shared_ptr<FactBase> &factBase);
		bool consult(const std::shared_ptr<RuleBase> &ruleBase);
		bool consultIntoUser(const std::filesystem::path &prologFile);
		
		std::shared_ptr<QueryResult> oneSolution1(const std::shared_ptr<Query> &goal);
		std::shared_ptr<QueryResult> oneSolution1(const std::shared_ptr<Query> &goal, const char *moduleName);
		std::list<std::shared_ptr<QueryResult>> allSolutions1(const std::shared_ptr<Query> &goal);

		void initializeProlog();
		virtual bool initializeDefaultPackages();
		
		PrologThreadPool& threadPool();
		
		friend class PrologReasoner::Runner;
	};

	/**
	 * A baseclass for prolog test fixtures.
	 */
	class PrologTests: public testing::Test {
	protected:
		static void runPrologTests(const std::shared_ptr<knowrob::PrologReasoner> &reasoner,
								   const std::string &target);
	};
}

#endif //KNOWROB_PROLOG_REASONER_H_
