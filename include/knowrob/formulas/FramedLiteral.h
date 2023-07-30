//
// Created by daniel on 07.04.23.
//

#ifndef KNOWROB_FRAMED_LITERAL_H
#define KNOWROB_FRAMED_LITERAL_H

#include "knowrob/formulas/Predicate.h"
#include "knowrob/semweb/StatementData.h"
#include "knowrob/terms/Constant.h"
#include "Literal.h"
#include "knowrob/modalities/ModalityFrame.h"

namespace knowrob {
    /**
     * A triple expression where subject, predicate and object are
     * represented as a term, and an additional unary operator can be applied to the object.
     */
    class FramedLiteral {
    public:
        /**
         * Unary operators that can be applied on terms.
         */
        enum OperatorType { EQ, LT, GT, LEQ, GEQ };

        explicit FramedLiteral(const LiteralPtr &literal, const ModalityFrame &modalityFrame=ModalityFrame());

        explicit FramedLiteral(const StatementData &tripleData);

        FramedLiteral(const TermPtr &subjectTerm,
                      const TermPtr &propertyTerm,
                      const TermPtr &objectTerm,
                      OperatorType objectOperator=EQ,
                      const std::string_view &graphName="*");

/*
        explicit FramedLiteral(const PredicatePtr &triplePredicate,
                               const std::string_view &graphName="*");
*/

        /**
         * @return true if the expression has no variables.
         */
        bool isGround() const;

        /**
         * @return the subject term of this expression.
         */
        std::shared_ptr<Term> subjectTerm() const;

        /**
         * @return the property term of this expression.
         */
        std::shared_ptr<Term> propertyTerm() const;

        /**
         * @return the object term of this expression.
         */
        std::shared_ptr<Term> objectTerm() const;

        /**
         * @return the graph term of this expression.
         */
        std::shared_ptr<Term> graphTerm() const;

        /**
         * @return the agent term of this expression.
         */
        std::shared_ptr<Term> agentTerm() const;

        /**
         * @return the begin term of this expression.
         */
        std::shared_ptr<Term> beginTerm() const;

        /**
         * @return the end term of this expression.
         */
        std::shared_ptr<Term> endTerm() const;

        /**
         * @return the confidence term of this expression.
         */
        std::shared_ptr<Term> confidenceTerm() const;

        /**
         * @return the operator for the object of the triple.
         */
        OperatorType objectOperator() const;

        /**
         * @return the operator for the confidence of the triple.
         */
        OperatorType confidenceOperator() const;

        /**
         * @return the operator for the begin of the triple.
         */
        OperatorType beginOperator() const;

        /**
         * @return the operator for the end of the triple.
         */
        OperatorType endOperator() const;

        /**
         * @param limit the minimum confidence of triples matching this expression
         */
        void setMinConfidence(double limit);

        /**
         * @param limit the maximum confidence of triples matching this expression
         */
        void setMaxConfidence(double limit);

        /**
         * @param limit the minimum begin time of triples matching this expression
         */
        void setMinBegin(double limit);

        /**
         * @param limit the maximum begin time of triples matching this expression
         */
        void setMaxBegin(double limit);

        /**
         * @param limit the minimum end time of triples matching this expression
         */
        void setMinEnd(double limit);

        /**
         * @param limit the maximum end time of triples matching this expression
         */
        void setMaxEnd(double limit);

        /**
         * @param beginOperator the operator used for the begin time of triples.
         */
        void setBeginOperator(OperatorType beginOperator);

        /**
         * @param beginOperator the operator used for the end time of triples.
         */
        void setEndOperator(OperatorType endOperator);

        /**
         * @param beginTerm a time term.
         */
        void setBeginTerm(const TermPtr &beginTerm);

        /**
         * @param endTerm a time term.
         */
        void setEndTerm(const TermPtr &endTerm);

        void setAgentTerm(const std::string &agentTerm);

    protected:
        ModalityFrame modalityFrame_;
        LiteralPtr  literal_;

        TermPtr subjectTerm_;
        TermPtr propertyTerm_;
        TermPtr objectTerm_;
        TermPtr graphTerm_;
        TermPtr agentTerm_;
        TermPtr beginTerm_;
        TermPtr endTerm_;
        TermPtr confidenceTerm_;
        OperatorType objectOperator_;
        OperatorType beginOperator_;
        OperatorType endOperator_;
        OperatorType confidenceOperator_;
    };
    using FramedLiteralPtr = std::shared_ptr<FramedLiteral>;

} // knowrob

#endif //KNOWROB_FRAMED_LITERAL_H