/*
 * Copyright (c) 2022, Daniel Beßler
 * All rights reserved.
 *
 * This file is part of KnowRob, please consult
 * https://github.com/knowrob/knowrob for license details.
 */

#include "knowrob/terms/Substitution.h"
#include "knowrob/terms/Unifier.h"

using namespace knowrob;

void Reversible::rollBack()
{
	while(!empty()) {
		auto &revertFunction = front();
		revertFunction();
		pop();
	}
}

void Substitution::set(const Variable &var, const TermPtr &term)
{
	mapping_.insert(std::pair<Variable,TermPtr>(var, term));
}

bool Substitution::contains(const Variable &var) const
{
	return mapping_.find(var) != mapping_.end();
}

const TermPtr& Substitution::get(const Variable &var) const
{
	static const TermPtr null_term;
	
	auto it = mapping_.find(var);
	if(it != mapping_.end()) {
		return it->second;
	}
	else {
		return null_term;
	}
}

const TermPtr& Substitution::get(const std::string &varName) const
{
	return get(Variable(varName));
}

size_t Substitution::computeHash() const
{
    static const auto GOLDEN_RATIO_HASH = static_cast<size_t>(0x9e3779b9);

    auto seed = static_cast<size_t>(0);

    for(const auto &item : mapping_) {
        // Compute the hash of the key using the in-built hash function for string.
        auto key_hash = item.first.computeHash();

        auto value_hash = static_cast<size_t>(0);
        if (item.second) {
            value_hash = item.second->computeHash();
        }

        /* Combine the hashes.
           The function (a ^ (b + GOLDEN_RATIO_HASH + (a << 6) + (a >> 2))) is known to
           give a good distribution of hash values across the range of size_t.
           The `<< 6` and `>> 2` operations are bitwise shifts which also help to distribute the
           hash values evenly by spreading out the influence of each bit in the input seed.
           This helps to minimize collisions and makes the hash value sensitive to changes in
           any part of the seed. */
        seed ^= key_hash + GOLDEN_RATIO_HASH + (seed << 6) + (seed >> 2);
        seed ^= value_hash + GOLDEN_RATIO_HASH + (seed << 6) + (seed >> 2);
    }

    return seed;
}

bool Substitution::unifyWith(const Substitution &other, Reversible *reversible)
{
	for(const auto &pair : other.mapping_) {
		auto it = mapping_.find(pair.first);
		if(it == mapping_.end()) {
			// new variable instantiation
			auto jt = mapping_.insert(pair).first;
			if(reversible) reversible->push(([this,jt](){ mapping_.erase(jt); }));
		}
		else {
			// variable has already an instantiation, need to unify
			TermPtr t0 = it->second;
			TermPtr t1 = pair.second;
			
			// t0 and t1 are not syntactically equal -> compute a unifier
			Unifier sigma(t0,t1);
			if(sigma.exists()) {
				// a unifier exists
				it->second = sigma.apply();
				if(reversible) reversible->push(([it,t0](){ it->second = t0; }));
			}
			else {
				// no unifier exists
				return false;
			}
		}
	}
	
	return true;
}


namespace std {
	std::ostream& operator<<(std::ostream& os, const Substitution& omega) //NOLINT
	{
		uint32_t i=0;
		os << '{';
		for(const auto &pair : omega) {
			if(i++ > 0) os << ',';
			os << pair.first.name() << ':' << ' ' << (*pair.second);
		}
		return os << '}';
	}
}
