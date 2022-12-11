/*
 * Copyright (c) 2022, Daniel Beßler
 * All rights reserved.
 *
 * This file is part of KnowRob, please consult
 * https://github.com/knowrob/knowrob for license details.
 */

// logging
#include <spdlog/spdlog.h>
// KnowRob
#include <knowrob/lang/terms.h>

using namespace knowrob;

Term::Term(TermType type)
: type_(type)
{}

bool Term::isBottom() const
{
	return type_ == TermType::BOTTOM;
}

bool Term::isTop() const
{
	return type_ == TermType::TOP;
}

std::ostream& operator<<(std::ostream& os, const Term& t)
{
	t.write(os);
	return os;
}


const std::shared_ptr<TopTerm>& TopTerm::get()
{
	static auto singleton = std::shared_ptr<TopTerm>(new TopTerm);
	return singleton;
}

void TopTerm::write(std::ostream& os) const
{
	os << "\u22A4";
}


const std::shared_ptr<BottomTerm>& BottomTerm::get()
{
	static auto singleton = std::shared_ptr<BottomTerm>(new BottomTerm);
	return singleton;
}

void BottomTerm::write(std::ostream& os) const
{
	os << "\u22A5";
}


Variable::Variable(const std::string &name)
: Term(TermType::VARIABLE),
  name_(name)
{}

bool Variable::operator< (const Variable& other) const
{
	return (this->name_ < other.name_);
}

void Variable::write(std::ostream& os) const
{
	os << "var" << '(' << name_ << ')';
}


StringTerm::StringTerm(const std::string &v)
: Constant(TermType::STRING, v)
{}

DoubleTerm::DoubleTerm(const double &v)
: Constant(TermType::DOUBLE, v)
{}
	
LongTerm::LongTerm(const long &v)
: Constant(TermType::LONG, v)
{}
	
Integer32Term::Integer32Term(const int32_t &v)
: Constant(TermType::INT32, v)
{}


PredicateIndicator::PredicateIndicator(const std::string &functor, unsigned int arity)
: functor_(functor),
  arity_(arity)
{}

bool PredicateIndicator::operator< (const PredicateIndicator& other) const
{
	return (other.functor_ < this->functor_) ||
	       (other.arity_   < this->arity_);
}

void PredicateIndicator::write(std::ostream& os) const
{
	os << functor_ << '/' << arity_;
}


Predicate::Predicate(
	const std::shared_ptr<PredicateIndicator> &indicator,
	const std::vector<std::shared_ptr<Term>> &arguments)
: Term(TermType::PREDICATE),
  indicator_(indicator),
  arguments_(arguments),
  isGround_(isGround1())
{
}

Predicate::Predicate(const Predicate &other, const Substitution &sub)
: Term(TermType::PREDICATE),
  indicator_(other.indicator_),
  arguments_(applySubstitution(other.arguments_, sub)),
  isGround_(isGround1())
{
}

Predicate::Predicate(
	const std::string &functor,
	const std::vector<std::shared_ptr<Term>> &arguments)
: Predicate(
	std::shared_ptr<PredicateIndicator>(
		new PredicateIndicator(functor, arguments.size())
	),
	arguments)
{
}

bool Predicate::isGround1() const
{
	for(const auto &x : arguments_) {
		if(!x->isGround()) return false;
	}
	return true;
}

std::vector<std::shared_ptr<Term>> Predicate::applySubstitution(
	const std::vector<std::shared_ptr<Term>> &in,
	const Substitution &sub) const
{
	std::vector<std::shared_ptr<Term>> out(in.size());
	
	for(uint32_t i=0; i<in.size(); i++) {
		switch(in[i]->type()) {
		case TermType::VARIABLE: {
			// replace variable argument if included in the substitution mapping
			std::shared_ptr<Term> t = sub.get(*((Variable*) in[i].get()));
			if(t.get() == NULL) {
				// variable is not included in the substitution, keep it
				out[i] = in[i];
			} else {
				// replace variable with term
				out[i] = t;
			}
			break;
		}
		case TermType::PREDICATE: {
			// recursively apply substitution
			Predicate *p = (Predicate*)in[i].get();
			out[i] = (p->isGround() ?
				in[i] :
				p->applySubstitution(sub));
			break;
		}
		default:
			out[i] = in[i];
			break;
		}
	}
	
	return out;
}

std::shared_ptr<Predicate> Predicate::applySubstitution(const Substitution &sub) const
{
	return std::shared_ptr<Predicate>(new Predicate(*this, sub));
}

void Predicate::write(std::ostream& os) const
{
	os << indicator_->functor() << '(';
	for(uint32_t i=0; i<arguments_.size(); i++) {
		Term *t = arguments_[i].get();
		os << (*t);
		if(i+1 < arguments_.size()) {
			os << ',' << ' ';
		}
	}
	os << ')';
}


void Substitution::set(const Variable &var, const std::shared_ptr<Term> &term)
{
	mapping_[var] = term;
}

bool Substitution::contains(const Variable &var) const
{
	return mapping_.find(var) != mapping_.end();
}

void Substitution::erase(const Variable &var)
{
	mapping_.erase(var);
}

std::shared_ptr<Term> Substitution::get(const Variable &var) const
{
	static const std::shared_ptr<Term> null_term;
	
	auto it = mapping_.find(var);
	if(it != mapping_.end()) {
		return it->second;
	}
	else {
		return null_term;
	}
}

bool Substitution::combine(const std::shared_ptr<Substitution> &other, Substitution::Diff &changes)
{
	for(const auto &pair : other->mapping_) {
		auto it = mapping_.find(pair.first);
		if(it == mapping_.end()) {
			// new variable instantiation
			changes.push(std::shared_ptr<Operation>(
				new Added(mapping_.insert(pair).first)));
		}
		else {
			// variable has already an instantation, need to unify
			std::shared_ptr<Term> t0 = it->second;
			std::shared_ptr<Term> t1 = pair.second;
			
			// t0 and t1 are not syntactically equal -> compute a unifier
			Unifier sigma(t0,t1);
			if(sigma.exists()) {
				// unifier exists
				it->second = sigma.apply();
				changes.push(std::shared_ptr<Operation>(new Replaced(it, t0)));
			}
			else {
				// no unifier exists
				return false;
			}
		}
	}
	
	return true;
}

void Substitution::rollBack(Substitution::Diff &changes)
{
	while(!changes.empty()) {
		auto &change = changes.front();
		change->rollBack(*this);
		changes.pop();
	}
}


Substitution::Replaced::Replaced(const Substitution::Iterator &it,
	const std::shared_ptr<Term> &replacedInstance)
: Substitution::Operation(),
  it_(it),
  replacedInstance_(replacedInstance)
{}

void Substitution::Replaced::rollBack(Substitution &sub)
{
	// set old value
	it_->second = replacedInstance_;
}


Substitution::Added::Added(const Substitution::Iterator &it)
: Substitution::Operation(),
  it_(it)
{}

void Substitution::Added::rollBack(Substitution &sub)
{
	// remove the added variable instantiation
	sub.mapping_.erase(it_);
}


Unifier::Unifier(const std::shared_ptr<Term> &t0, const std::shared_ptr<Term> &t1)
: Substitution(),
  t0_(t0),
  t1_(t1),
  exists_(unify(t0,t1))
{
}

bool Unifier::unify(const std::shared_ptr<Term> &t0, const std::shared_ptr<Term> &t1)
{
	// TODO: use fast equals test before computing the unifier?
	/*
	if(t0->equals(t1)) {
		// terms are already equal
		return true;
	}
	*/
	
	if(t1->type() == TermType::VARIABLE) {
		set(*((Variable*)t1.get()), t0);
	}
	else {
		switch(t0->type()) {
		case TermType::VARIABLE:
			set(*((Variable*)t0.get()), t1);
			break;
		case TermType::PREDICATE: {
			// predicates only unify with other predicates
			if(t1->type()!=TermType::PREDICATE) {
				return false;
			}
			Predicate *p0 = (Predicate*)t0.get();
			Predicate *p1 = (Predicate*)t1.get();
			// test for functor equality, and matching arity
			if(p0->indicator().functor() != p0->indicator().functor() ||
			   p0->indicator().arity()   != p0->indicator().arity()) {
				return false;
			}
			// unify all arguments
			for(uint32_t i=0; i<p0->indicator().arity(); ++i) {
				if(!unify(p0->arguments()[i], p1->arguments()[i])) {
					return false;
				}
			}
			break;
		}
		case TermType::STRING:
			return t1->type()==TermType::STRING &&
				((StringTerm*)t0.get())->value()==((StringTerm*)t1.get())->value();
		case TermType::DOUBLE:
			return t1->type()==TermType::DOUBLE &&
				((DoubleTerm*)t0.get())->value()==((DoubleTerm*)t1.get())->value();
		case TermType::INT32:
			return t1->type()==TermType::INT32 &&
				((Integer32Term*)t0.get())->value()==((Integer32Term*)t1.get())->value();
		case TermType::LONG:
			return t1->type()==TermType::LONG &&
				((LongTerm*)t0.get())->value()==((LongTerm*)t1.get())->value();
		case TermType::TOP:
			return t1->isTop();
		case TermType::BOTTOM:
			return t1->isBottom();
		default:
			spdlog::warn("Ignoring unknown term type '{}'.", t0->type());
			return false;
		}
	}
	
	return true;
}

std::shared_ptr<Term> Unifier::apply()
{
	if(!exists_) {
		// no unifier exists
		return BottomTerm::get();
	}
	else if(mapping_.empty() ||
		t0_->isGround() ||
		t1_->type()==TermType::VARIABLE)
	{
		// empty unifier, or only substitutions in t1
		return t0_;
	}
	else if(t1_->isGround() ||
		t0_->type()==TermType::VARIABLE)
	{
		// only substitutions in t0
		return t1_;
	}
	else if(t0_->type()==TermType::PREDICATE) {
		// both t0_ and t1_ contain variables, so they are either Variables
		// or Predicates where an argument contains a variable.
		// the variable case if covered above so both must be predicates.
		// TODO: choose the one with less variables?
		Predicate *p = (Predicate*)t0_.get();
		return p->applySubstitution(*this);
	}
	else {
		spdlog::warn("something went wrong.");
		return BottomTerm::get();
	}
}

