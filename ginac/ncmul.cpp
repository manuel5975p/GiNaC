/** @file ncmul.cpp
 *
 *  Implementation of GiNaC's non-commutative products of expressions. */

/*
 *  GiNaC Copyright (C) 1999-2002 Johannes Gutenberg University Mainz, Germany
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <algorithm>
#include <iostream>
#include <stdexcept>

#include "ncmul.h"
#include "ex.h"
#include "add.h"
#include "mul.h"
#include "matrix.h"
#include "print.h"
#include "archive.h"
#include "utils.h"

namespace GiNaC {

GINAC_IMPLEMENT_REGISTERED_CLASS(ncmul, exprseq)

//////////
// default ctor, dtor, copy ctor, assignment operator and helpers
//////////

ncmul::ncmul()
{
	tinfo_key = TINFO_ncmul;
}

DEFAULT_COPY(ncmul)
DEFAULT_DESTROY(ncmul)

//////////
// other constructors
//////////

// public

ncmul::ncmul(const ex & lh, const ex & rh) : inherited(lh,rh)
{
	tinfo_key = TINFO_ncmul;
}

ncmul::ncmul(const ex & f1, const ex & f2, const ex & f3) : inherited(f1,f2,f3)
{
	tinfo_key = TINFO_ncmul;
}

ncmul::ncmul(const ex & f1, const ex & f2, const ex & f3,
             const ex & f4) : inherited(f1,f2,f3,f4)
{
	tinfo_key = TINFO_ncmul;
}

ncmul::ncmul(const ex & f1, const ex & f2, const ex & f3,
             const ex & f4, const ex & f5) : inherited(f1,f2,f3,f4,f5)
{
	tinfo_key = TINFO_ncmul;
}

ncmul::ncmul(const ex & f1, const ex & f2, const ex & f3,
             const ex & f4, const ex & f5, const ex & f6) : inherited(f1,f2,f3,f4,f5,f6)
{
	tinfo_key = TINFO_ncmul;
}

ncmul::ncmul(const exvector & v, bool discardable) : inherited(v,discardable)
{
	tinfo_key = TINFO_ncmul;
}

ncmul::ncmul(exvector * vp) : inherited(vp)
{
	tinfo_key = TINFO_ncmul;
}

//////////
// archiving
//////////

DEFAULT_ARCHIVING(ncmul)
	
//////////
// functions overriding virtual functions from base classes
//////////

// public

void ncmul::print(const print_context & c, unsigned level) const
{
	if (is_a<print_tree>(c)) {

		inherited::print(c, level);

	} else if (is_a<print_csrc>(c) || is_a<print_python_repr>(c)) {

		c.s << class_name() << "(";
		exvector::const_iterator it = seq.begin(), itend = seq.end()-1;
		while (it != itend) {
			it->print(c, precedence());
			c.s << ",";
			it++;
		}
		it->print(c, precedence());
		c.s << ")";

	} else
		printseq(c, '(', '*', ')', precedence(), level);
}

bool ncmul::info(unsigned inf) const
{
	return inherited::info(inf);
}

typedef std::vector<int> intvector;

ex ncmul::expand(unsigned options) const
{
	// First, expand the children
	exvector expanded_seq = expandchildren(options);
	
	// Now, look for all the factors that are sums and remember their
	// position and number of terms.
	intvector positions_of_adds(expanded_seq.size());
	intvector number_of_add_operands(expanded_seq.size());

	int number_of_adds = 0;
	int number_of_expanded_terms = 1;

	unsigned current_position = 0;
	exvector::const_iterator last = expanded_seq.end();
	for (exvector::const_iterator cit=expanded_seq.begin(); cit!=last; ++cit) {
		if (is_exactly_a<add>(*cit)) {
			positions_of_adds[number_of_adds] = current_position;
			unsigned num_ops = cit->nops();
			number_of_add_operands[number_of_adds] = num_ops;
			number_of_expanded_terms *= num_ops;
			number_of_adds++;
		}
		++current_position;
	}

	// If there are no sums, we are done
	if (number_of_adds == 0)
		return (new ncmul(expanded_seq, true))->
		        setflag(status_flags::dynallocated | (options == 0 ? status_flags::expanded : 0));

	// Now, form all possible products of the terms of the sums with the
	// remaining factors, and add them together
	exvector distrseq;
	distrseq.reserve(number_of_expanded_terms);

	intvector k(number_of_adds);

	while (true) {
		exvector term = expanded_seq;
		for (int i=0; i<number_of_adds; i++)
			term[positions_of_adds[i]] = expanded_seq[positions_of_adds[i]].op(k[i]);
		distrseq.push_back((new ncmul(term, true))->
		                    setflag(status_flags::dynallocated | (options == 0 ? status_flags::expanded : 0)));

		// increment k[]
		int l = number_of_adds-1;
		while ((l>=0) && ((++k[l]) >= number_of_add_operands[l])) {
			k[l] = 0;
			l--;
		}
		if (l<0)
			break;
	}

	return (new add(distrseq))->
	        setflag(status_flags::dynallocated | (options == 0 ? status_flags::expanded : 0));
}

int ncmul::degree(const ex & s) const
{
	// Sum up degrees of factors
	int deg_sum = 0;
	exvector::const_iterator i = seq.begin(), end = seq.end();
	while (i != end) {
		deg_sum += i->degree(s);
		++i;
	}
	return deg_sum;
}

int ncmul::ldegree(const ex & s) const
{
	// Sum up degrees of factors
	int deg_sum = 0;
	exvector::const_iterator i = seq.begin(), end = seq.end();
	while (i != end) {
		deg_sum += i->degree(s);
		++i;
	}
	return deg_sum;
}

ex ncmul::coeff(const ex & s, int n) const
{
	exvector coeffseq;
	coeffseq.reserve(seq.size());

	if (n == 0) {
		// product of individual coeffs
		// if a non-zero power of s is found, the resulting product will be 0
		exvector::const_iterator it=seq.begin();
		while (it!=seq.end()) {
			coeffseq.push_back((*it).coeff(s,n));
			++it;
		}
		return (new ncmul(coeffseq,1))->setflag(status_flags::dynallocated);
	}
		 
	exvector::const_iterator i = seq.begin(), end = seq.end();
	bool coeff_found = false;
	while (i != end) {
		ex c = i->coeff(s,n);
		if (c.is_zero()) {
			coeffseq.push_back(*i);
		} else {
			coeffseq.push_back(c);
			coeff_found = true;
		}
		++i;
	}

	if (coeff_found) return (new ncmul(coeffseq,1))->setflag(status_flags::dynallocated);
	
	return _ex0;
}

unsigned ncmul::count_factors(const ex & e) const
{
	if ((is_ex_exactly_of_type(e,mul)&&(e.return_type()!=return_types::commutative))||
		(is_ex_exactly_of_type(e,ncmul))) {
		unsigned factors=0;
		for (unsigned i=0; i<e.nops(); i++)
			factors += count_factors(e.op(i));
		
		return factors;
	}
	return 1;
}
		
void ncmul::append_factors(exvector & v, const ex & e) const
{
	if ((is_ex_exactly_of_type(e,mul)&&(e.return_type()!=return_types::commutative))||
		(is_ex_exactly_of_type(e,ncmul))) {
		for (unsigned i=0; i<e.nops(); i++)
			append_factors(v,e.op(i));
	} else 
		v.push_back(e);
}

typedef std::vector<unsigned> unsignedvector;
typedef std::vector<exvector> exvectorvector;

/** Perform automatic term rewriting rules in this class.  In the following
 *  x, x1, x2,... stand for a symbolic variables of type ex and c, c1, c2...
 *  stand for such expressions that contain a plain number.
 *  - ncmul(...,*(x1,x2),...,ncmul(x3,x4),...) -> ncmul(...,x1,x2,...,x3,x4,...)  (associativity)
 *  - ncmul(x) -> x
 *  - ncmul() -> 1
 *  - ncmul(...,c1,...,c2,...) -> *(c1,c2,ncmul(...))  (pull out commutative elements)
 *  - ncmul(x1,y1,x2,y2) -> *(ncmul(x1,x2),ncmul(y1,y2))  (collect elements of same type)
 *  - ncmul(x1,x2,x3,...) -> x::simplify_ncmul(x1,x2,x3,...)
 *
 *  @param level cut-off in recursive evaluation */
ex ncmul::eval(int level) const
{
	// The following additional rule would be nice, but produces a recursion,
	// which must be trapped by introducing a flag that the sub-ncmuls()
	// are already evaluated (maybe later...)
	//                  ncmul(x1,x2,...,X,y1,y2,...) ->
	//                      ncmul(ncmul(x1,x2,...),X,ncmul(y1,y2,...)
	//                      (X noncommutative_composite)

	if ((level==1) && (flags & status_flags::evaluated)) {
		return *this;
	}

	exvector evaledseq=evalchildren(level);

	// ncmul(...,*(x1,x2),...,ncmul(x3,x4),...) ->
	//     ncmul(...,x1,x2,...,x3,x4,...)  (associativity)
	unsigned factors = 0;
	exvector::const_iterator cit = evaledseq.begin(), citend = evaledseq.end();
	while (cit != citend)
		factors += count_factors(*cit++);
	
	exvector assocseq;
	assocseq.reserve(factors);
	cit = evaledseq.begin();
	while (cit != citend)
		append_factors(assocseq, *cit++);
	
	// ncmul(x) -> x
	if (assocseq.size()==1) return *(seq.begin());

	// ncmul() -> 1
	if (assocseq.empty()) return _ex1;

	// determine return types
	unsignedvector rettypes;
	rettypes.reserve(assocseq.size());
	unsigned i = 0;
	unsigned count_commutative=0;
	unsigned count_noncommutative=0;
	unsigned count_noncommutative_composite=0;
	cit = assocseq.begin(); citend = assocseq.end();
	while (cit != citend) {
		switch (rettypes[i] = cit->return_type()) {
		case return_types::commutative:
			count_commutative++;
			break;
		case return_types::noncommutative:
			count_noncommutative++;
			break;
		case return_types::noncommutative_composite:
			count_noncommutative_composite++;
			break;
		default:
			throw(std::logic_error("ncmul::eval(): invalid return type"));
		}
		++i; ++cit;
	}
	GINAC_ASSERT(count_commutative+count_noncommutative+count_noncommutative_composite==assocseq.size());

	// ncmul(...,c1,...,c2,...) ->
	//     *(c1,c2,ncmul(...)) (pull out commutative elements)
	if (count_commutative!=0) {
		exvector commutativeseq;
		commutativeseq.reserve(count_commutative+1);
		exvector noncommutativeseq;
		noncommutativeseq.reserve(assocseq.size()-count_commutative);
		unsigned num = assocseq.size();
		for (unsigned i=0; i<num; ++i) {
			if (rettypes[i]==return_types::commutative)
				commutativeseq.push_back(assocseq[i]);
			else
				noncommutativeseq.push_back(assocseq[i]);
		}
		commutativeseq.push_back((new ncmul(noncommutativeseq,1))->setflag(status_flags::dynallocated));
		return (new mul(commutativeseq))->setflag(status_flags::dynallocated);
	}
		
	// ncmul(x1,y1,x2,y2) -> *(ncmul(x1,x2),ncmul(y1,y2))
	//     (collect elements of same type)

	if (count_noncommutative_composite==0) {
		// there are neither commutative nor noncommutative_composite
		// elements in assocseq
		GINAC_ASSERT(count_commutative==0);

		unsigned assoc_num = assocseq.size();
		exvectorvector evv;
		unsignedvector rttinfos;
		evv.reserve(assoc_num);
		rttinfos.reserve(assoc_num);

		cit = assocseq.begin(), citend = assocseq.end();
		while (cit != citend) {
			unsigned ti = cit->return_type_tinfo();
			unsigned rtt_num = rttinfos.size();
			// search type in vector of known types
			for (i=0; i<rtt_num; ++i) {
				if (ti == rttinfos[i]) {
					evv[i].push_back(*cit);
					break;
				}
			}
			if (i >= rtt_num) {
				// new type
				rttinfos.push_back(ti);
				evv.push_back(exvector());
				(evv.end()-1)->reserve(assoc_num);
				(evv.end()-1)->push_back(*cit);
			}
			++cit;
		}

		unsigned evv_num = evv.size();
#ifdef DO_GINAC_ASSERT
		GINAC_ASSERT(evv_num == rttinfos.size());
		GINAC_ASSERT(evv_num > 0);
		unsigned s=0;
		for (i=0; i<evv_num; ++i)
			s += evv[i].size();
		GINAC_ASSERT(s == assoc_num);
#endif // def DO_GINAC_ASSERT
		
		// if all elements are of same type, simplify the string
		if (evv_num == 1)
			return evv[0][0].simplify_ncmul(evv[0]);
		
		exvector splitseq;
		splitseq.reserve(evv_num);
		for (i=0; i<evv_num; ++i)
			splitseq.push_back((new ncmul(evv[i]))->setflag(status_flags::dynallocated));
		
		return (new mul(splitseq))->setflag(status_flags::dynallocated);
	}
	
	return (new ncmul(assocseq))->setflag(status_flags::dynallocated |
										  status_flags::evaluated);
}

ex ncmul::evalm(void) const
{
	// Evaluate children first
	exvector *s = new exvector;
	s->reserve(seq.size());
	exvector::const_iterator it = seq.begin(), itend = seq.end();
	while (it != itend) {
		s->push_back(it->evalm());
		it++;
	}

	// If there are only matrices, simply multiply them
	it = s->begin(); itend = s->end();
	if (is_ex_of_type(*it, matrix)) {
		matrix prod(ex_to<matrix>(*it));
		it++;
		while (it != itend) {
			if (!is_ex_of_type(*it, matrix))
				goto no_matrix;
			prod = prod.mul(ex_to<matrix>(*it));
			it++;
		}
		delete s;
		return prod;
	}

no_matrix:
	return (new ncmul(s))->setflag(status_flags::dynallocated);
}

ex ncmul::thisexprseq(const exvector & v) const
{
	return (new ncmul(v))->setflag(status_flags::dynallocated);
}

ex ncmul::thisexprseq(exvector * vp) const
{
	return (new ncmul(vp))->setflag(status_flags::dynallocated);
}

// protected

/** Implementation of ex::diff() for a non-commutative product. It applies
 *  the product rule.
 *  @see ex::diff */
ex ncmul::derivative(const symbol & s) const
{
	unsigned num = seq.size();
	exvector addseq;
	addseq.reserve(num);
	
	// D(a*b*c) = D(a)*b*c + a*D(b)*c + a*b*D(c)
	exvector ncmulseq = seq;
	for (unsigned i=0; i<num; ++i) {
		ex e = seq[i].diff(s);
		e.swap(ncmulseq[i]);
		addseq.push_back((new ncmul(ncmulseq))->setflag(status_flags::dynallocated));
		e.swap(ncmulseq[i]);
	}
	return (new add(addseq))->setflag(status_flags::dynallocated);
}

int ncmul::compare_same_type(const basic & other) const
{
	return inherited::compare_same_type(other);
}

unsigned ncmul::return_type(void) const
{
	if (seq.empty())
		return return_types::commutative;

	bool all_commutative = true;
	exvector::const_iterator noncommutative_element; // point to first found nc element

	exvector::const_iterator i = seq.begin(), end = seq.end();
	while (i != end) {
		unsigned rt = i->return_type();
		if (rt == return_types::noncommutative_composite)
			return rt; // one ncc -> mul also ncc
		if ((rt == return_types::noncommutative) && (all_commutative)) {
			// first nc element found, remember position
			noncommutative_element = i;
			all_commutative = false;
		}
		if ((rt == return_types::noncommutative) && (!all_commutative)) {
			// another nc element found, compare type_infos
			if (noncommutative_element->return_type_tinfo() != i->return_type_tinfo()) {
				// diffent types -> mul is ncc
				return return_types::noncommutative_composite;
			}
		}
		++i;
	}
	// all factors checked
	GINAC_ASSERT(!all_commutative); // not all factors should commute, because this is a ncmul();
	return all_commutative ? return_types::commutative : return_types::noncommutative;
}
   
unsigned ncmul::return_type_tinfo(void) const
{
	if (seq.empty())
		return tinfo_key;

	// return type_info of first noncommutative element
	exvector::const_iterator i = seq.begin(), end = seq.end();
	while (i != end) {
		if (i->return_type() == return_types::noncommutative)
			return i->return_type_tinfo();
		++i;
	}

	// no noncommutative element found, should not happen
	return tinfo_key;
}

//////////
// new virtual functions which can be overridden by derived classes
//////////

// none

//////////
// non-virtual functions in this class
//////////

exvector ncmul::expandchildren(unsigned options) const
{
	exvector s;
	s.reserve(seq.size());
	exvector::const_iterator it = seq.begin(), itend = seq.end();
	while (it != itend) {
		s.push_back(it->expand(options));
		it++;
	}
	return s;
}

const exvector & ncmul::get_factors(void) const
{
	return seq;
}

//////////
// friend functions
//////////

ex nonsimplified_ncmul(const exvector & v)
{
	return (new ncmul(v))->setflag(status_flags::dynallocated);
}

ex simplified_ncmul(const exvector & v)
{
	if (v.empty())
		return _ex1;
	else if (v.size() == 1)
		return v[0];
	else
		return (new ncmul(v))->setflag(status_flags::dynallocated |
		                               status_flags::evaluated);
}

} // namespace GiNaC
