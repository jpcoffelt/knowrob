//
// Created by daniel on 12.03.23.
//

#ifndef KNOWROB_KNOWLEDGE_GRAPH_H
#define KNOWROB_KNOWLEDGE_GRAPH_H

#include <boost/property_tree/ptree.hpp>
#include "memory"
#include "optional"
#include "raptor2.h"
#include "knowrob/formulas/Literal.h"
#include "knowrob/queries/AnswerBuffer.h"
#include "knowrob/semweb/GraphQuery.h"
#include "knowrob/semweb/Vocabulary.h"
#include "knowrob/semweb/TripleExpression.h"
#include "knowrob/semweb/TripleData.h"
#include "knowrob/ThreadPool.h"

namespace knowrob {
    /**
     * Used to indicate the file format when loading triple data.
     */
    enum TripleFormat {
        RDF_XML,
        TURTLE,
        N_TRIPLES
    };

    class ITripleLoader {
    public:
        virtual void loadTriple(const TripleData &tripleData) = 0;
        virtual void flush() = 0;
    };

    class KnowledgeGraph {
    public:
        explicit KnowledgeGraph();

        KnowledgeGraph(const KnowledgeGraph&) = delete;

        ~KnowledgeGraph();

        /**
         * Initialize this KG from a property tree.
         * Note that each KG implementation has a different set of properties used
         * to configure it.
         * @param config a property tree.
         * @return true on success
         */
        virtual bool loadConfiguration(const boost::property_tree::ptree &config) = 0;

        /**
         * @return the vocabulary of this KG.
         */
        const auto& vocabulary() const { return vocabulary_; }

        /**
         * @param iri a RDF resource IRI
         * @return true if the KG contains a statement involving the resource IRI
         */
        bool isDefinedResource(const std::string_view &iri);

        /**
         * @param iri a RDF property IRI
         * @return true if the KG contains a statement involving the property IRI
         */
        bool isDefinedProperty(const std::string_view &iri);

        /**
         * @param iri a RDF class IRI
         * @return true if the KG contains a statement involving the class IRI
         */
        bool isDefinedClass(const std::string_view &iri);

        /**
         * Read RDF ontology from a remote URI or a local file, and load
         * triple data into the database knowledgeGraph.
         * @param uriString the URI pointing to a RDF file
         * @param format the format of the file
         * @return true if the file was loaded successfully
         */
        virtual bool loadTriples(
                const std::string_view &uriString,
                TripleFormat format,
                const std::optional<ModalIteration> &modality) = 0;

        bool loadTriples(const std::string_view &uriString, TripleFormat format)
        { return loadTriples(uriString, format, std::nullopt); }

        /**
         * Add an assertion to this KG.
         * @param tripleData data representing an atomic proposition.
         * @return true on success
         */
        virtual bool insert(const TripleData &tripleData) = 0;

        /**
         * Add assertions to this KG.
         * @param tripleData data representing atomic propositions.
         * @return true on success
         */
        virtual bool insert(const std::vector<TripleData> &tripleData) = 0;

        /**
         * Delete all matching statements from this KG.
         * @param tripleExpression an expression used to match statements in the KG.
         */
        virtual void removeAll(const semweb::TripleExpression &tripleExpression) = 0;

        /**
         * Delete the first matching statement from this KG.
         * @param tripleExpression an expression used to match statements in the KG.
         */
        virtual void removeOne(const semweb::TripleExpression &tripleExpression) = 0;

        /**
         * Submits a graph query to this knowledge graph.
         * The query is evaluated concurrently, and evaluation may still be active
         * when this function returns.
         * The function returns a stream of solutions, the end of the stream is indicated
         * by an EOS message.
         * @param query a graph query
         * @return a stream with answers to the query
         */
        AnswerBufferPtr submitQuery(const GraphQueryPtr &query);

        /**
         * Evaluates a query and may block until evaluation completed.
         * All results will be written into the provided stream object.
         * @param query a query.
         * @param resultStream a stream of answers.
         */
        virtual void evaluateQuery(const GraphQueryPtr &query, AnswerBufferPtr &resultStream) = 0;

        /**
         * Watch for instantiations of a literal in the knowledge graph.
         * @param literal a literal
         * @return a stream with answers to the query
         */
        virtual AnswerBufferPtr watchQuery(const GraphQueryPtr &query) = 0;

        //virtual bool unwatchQuery(const BufferedAnswerStreamPtr &queryStream) = 0;

        /**
         * Ontologies are loaded into named sub-graphs of the knowledge graph.
         * The name is generated from the URI in case of loading RDF files.
         * @param uriString A URI pointing to a RDF ontology.
         * @return a graph name for the ontology
         */
        static std::string getNameFromURI(const std::string &uriString);

        /**
         * Extract a version string from an ontology URI.
         * In case the URI points to a local file, the modification time of the file
         * is used as version.
         * For other URIs it is attempted to extract version information from the URI string,
         * if this fails, then the current day is used as a version string.
         * @param uriString A URI pointing to a RDF ontology.
         * @return a version string
         */
        static std::string getVersionFromURI(const std::string &uriString);

        static bool isVersionString(const std::string &versionString);

        /**
         * Assign thread pool to be used during query evaluation.
         * This is optional, and if not called a default one will be used.
         * @param threadPool a thread pool.
         */
        void setThreadPool(const std::shared_ptr<ThreadPool> &threadPool);

    protected:
        std::shared_ptr<ThreadPool> threadPool_;
        raptor_world *raptorWorld_;
        std::shared_ptr<semweb::Vocabulary> vocabulary_;

        bool loadURI(ITripleLoader &loader,
                     const std::string &uriString,
                     std::string &blankPrefix,
                     TripleFormat format);
    };

    using KnowledgeGraphPtr = std::shared_ptr<KnowledgeGraph>;

} // knowrob

#endif //KNOWROB_KNOWLEDGE_GRAPH_H
