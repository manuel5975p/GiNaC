/** @file matrix.cpp
 *
 *  Implementation of symbolic matrices */

/*
 *  GiNaC Copyright (C) 1999-2001 Johannes Gutenberg University Mainz, Germany
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
#include <map>
#include <stdexcept>

#include "matrix.h"
#include "numeric.h"
#include "lst.h"
#include "idx.h"
#include "indexed.h"
#include "power.h"
#include "symbol.h"
#include "normal.h"
#include "print.h"
#include "archive.h"
#include "utils.h"
#include "debugmsg.h"

namespace GiNaC {

GINAC_IMPLEMENT_REGISTERED_CLASS(matrix, basic)

//////////
// default ctor, dtor, copy ctor, assignment operator and helpers:
//////////

/** Default ctor.  Initializes to 1 x 1-dimensional zero-matrix. */
matrix::matrix() : inherited(TINFO_matrix), row(1), col(1)
{
	debugmsg("matrix default ctor",LOGLEVEL_CONSTRUCT);
	m.push_back(_ex0());
}

void matrix::copy(const matrix & other)
{
	inherited::copy(other);
	row = other.row;
	col = other.col;
	m = other.m;  // STL's vector copying invoked here
}

DEFAULT_DESTROY(matrix)

//////////
// other ctors
//////////

// public

/** Very common ctor.  Initializes to r x c-dimensional zero-matrix.
 *
 *  @param r number of rows
 *  @param c number of cols */
matrix::matrix(unsigned r, unsigned c)
  : inherited(TINFO_matrix), row(r), col(c)
{
	debugmsg("matrix ctor from unsigned,unsigned",LOGLEVEL_CONSTRUCT);
	m.resize(r*c, _ex0());
}

// protected

/** Ctor from representation, for internal use only. */
matrix::matrix(unsigned r, unsigned c, const exvector & m2)
  : inherited(TINFO_matrix), row(r), col(c), m(m2)
{
	debugmsg("matrix ctor from unsigned,unsigned,exvector",LOGLEVEL_CONSTRUCT);
}

/** Construct matrix from (flat) list of elements. If the list has fewer
 *  elements than the matrix, the remaining matrix elements are set to zero.
 *  If the list has more elements than the matrix, the excessive elements are
 *  thrown away. */
matrix::matrix(unsigned r, unsigned c, const lst & l)
  : inherited(TINFO_matrix), row(r), col(c)
{
	debugmsg("matrix ctor from unsigned,unsigned,lst",LOGLEVEL_CONSTRUCT);
	m.resize(r*c, _ex0());

	for (unsigned i=0; i<l.nops(); i++) {
		unsigned x = i % c;
		unsigned y = i / c;
		if (y >= r)
			break; // matrix smaller than list: throw away excessive elements
		m[y*c+x] = l.op(i);
	}
}

//////////
// archiving
//////////

matrix::matrix(const archive_node &n, const lst &sym_lst) : inherited(n, sym_lst)
{
	debugmsg("matrix ctor from archive_node", LOGLEVEL_CONSTRUCT);
	if (!(n.find_unsigned("row", row)) || !(n.find_unsigned("col", col)))
		throw (std::runtime_error("unknown matrix dimensions in archive"));
	m.reserve(row * col);
	for (unsigned int i=0; true; i++) {
		ex e;
		if (n.find_ex("m", e, sym_lst, i))
			m.push_back(e);
		else
			break;
	}
}

void matrix::archive(archive_node &n) const
{
	inherited::archive(n);
	n.add_unsigned("row", row);
	n.add_unsigned("col", col);
	exvector::const_iterator i = m.begin(), iend = m.end();
	while (i != iend) {
		n.add_ex("m", *i);
		++i;
	}
}

DEFAULT_UNARCHIVE(matrix)

//////////
// functions overriding virtual functions from bases classes
//////////

// public

void matrix::print(const print_context & c, unsigned level) const
{
	debugmsg("matrix print", LOGLEVEL_PRINT);

	if (is_of_type(c, print_tree)) {

		inherited::print(c, level);

	} else {

		c.s << "[";
		for (unsigned y=0; y<row-1; ++y) {
			c.s << "[";
			for (unsigned x=0; x<col-1; ++x) {
				m[y*col+x].print(c);
				c.s << ",";
			}
			m[col*(y+1)-1].print(c);
			c.s << "],";
		}
		c.s << "[";
		for (unsigned x=0; x<col-1; ++x) {
			m[(row-1)*col+x].print(c);
			c.s << ",";
		}
		m[row*col-1].print(c);
		c.s << "]]";

	}
}

/** nops is defined to be rows x columns. */
unsigned matrix::nops() const
{
	return row*col;
}

/** returns matrix entry at position (i/col, i%col). */
ex matrix::op(int i) const
{
	return m[i];
}

/** returns matrix entry at position (i/col, i%col). */
ex & matrix::let_op(int i)
{
	GINAC_ASSERT(i>=0);
	GINAC_ASSERT(i<nops());
	
	return m[i];
}

/** Evaluate matrix entry by entry. */
ex matrix::eval(int level) const
{
	debugmsg("matrix eval",LOGLEVEL_MEMBER_FUNCTION);
	
	// check if we have to do anything at all
	if ((level==1)&&(flags & status_flags::evaluated))
		return *this;
	
	// emergency break
	if (level == -max_recursion_level)
		throw (std::runtime_error("matrix::eval(): recursion limit exceeded"));
	
	// eval() entry by entry
	exvector m2(row*col);
	--level;
	for (unsigned r=0; r<row; ++r)
		for (unsigned c=0; c<col; ++c)
			m2[r*col+c] = m[r*col+c].eval(level);
	
	return (new matrix(row, col, m2))->setflag(status_flags::dynallocated |
											   status_flags::evaluated );
}

ex matrix::subs(const lst & ls, const lst & lr, bool no_pattern) const
{
	exvector m2(row * col);
	for (unsigned r=0; r<row; ++r)
		for (unsigned c=0; c<col; ++c)
			m2[r*col+c] = m[r*col+c].subs(ls, lr, no_pattern);

	return ex(matrix(row, col, m2)).bp->basic::subs(ls, lr, no_pattern);
}

// protected

int matrix::compare_same_type(const basic & other) const
{
	GINAC_ASSERT(is_exactly_of_type(other, matrix));
	const matrix & o = static_cast<const matrix &>(other);
	
	// compare number of rows
	if (row != o.rows())
		return row < o.rows() ? -1 : 1;
	
	// compare number of columns
	if (col != o.cols())
		return col < o.cols() ? -1 : 1;
	
	// equal number of rows and columns, compare individual elements
	int cmpval;
	for (unsigned r=0; r<row; ++r) {
		for (unsigned c=0; c<col; ++c) {
			cmpval = ((*this)(r,c)).compare(o(r,c));
			if (cmpval!=0) return cmpval;
		}
	}
	// all elements are equal => matrices are equal;
	return 0;
}

/** Automatic symbolic evaluation of an indexed matrix. */
ex matrix::eval_indexed(const basic & i) const
{
	GINAC_ASSERT(is_of_type(i, indexed));
	GINAC_ASSERT(is_ex_of_type(i.op(0), matrix));

	bool all_indices_unsigned = static_cast<const indexed &>(i).all_index_values_are(info_flags::nonnegint);

	// Check indices
	if (i.nops() == 2) {

		// One index, must be one-dimensional vector
		if (row != 1 && col != 1)
			throw (std::runtime_error("matrix::eval_indexed(): vector must have exactly 1 index"));

		const idx & i1 = ex_to<idx>(i.op(1));

		if (col == 1) {

			// Column vector
			if (!i1.get_dim().is_equal(row))
				throw (std::runtime_error("matrix::eval_indexed(): dimension of index must match number of vector elements"));

			// Index numeric -> return vector element
			if (all_indices_unsigned) {
				unsigned n1 = ex_to<numeric>(i1.get_value()).to_int();
				if (n1 >= row)
					throw (std::runtime_error("matrix::eval_indexed(): value of index exceeds number of vector elements"));
				return (*this)(n1, 0);
			}

		} else {

			// Row vector
			if (!i1.get_dim().is_equal(col))
				throw (std::runtime_error("matrix::eval_indexed(): dimension of index must match number of vector elements"));

			// Index numeric -> return vector element
			if (all_indices_unsigned) {
				unsigned n1 = ex_to<numeric>(i1.get_value()).to_int();
				if (n1 >= col)
					throw (std::runtime_error("matrix::eval_indexed(): value of index exceeds number of vector elements"));
				return (*this)(0, n1);
			}
		}

	} else if (i.nops() == 3) {

		// Two indices
		const idx & i1 = ex_to<idx>(i.op(1));
		const idx & i2 = ex_to<idx>(i.op(2));

		if (!i1.get_dim().is_equal(row))
			throw (std::runtime_error("matrix::eval_indexed(): dimension of first index must match number of rows"));
		if (!i2.get_dim().is_equal(col))
			throw (std::runtime_error("matrix::eval_indexed(): dimension of second index must match number of columns"));

		// Pair of dummy indices -> compute trace
		if (is_dummy_pair(i1, i2))
			return trace();

		// Both indices numeric -> return matrix element
		if (all_indices_unsigned) {
			unsigned n1 = ex_to<numeric>(i1.get_value()).to_int(), n2 = ex_to<numeric>(i2.get_value()).to_int();
			if (n1 >= row)
				throw (std::runtime_error("matrix::eval_indexed(): value of first index exceeds number of rows"));
			if (n2 >= col)
				throw (std::runtime_error("matrix::eval_indexed(): value of second index exceeds number of columns"));
			return (*this)(n1, n2);
		}

	} else
		throw (std::runtime_error("matrix::eval_indexed(): matrix must have exactly 2 indices"));

	return i.hold();
}

/** Sum of two indexed matrices. */
ex matrix::add_indexed(const ex & self, const ex & other) const
{
	GINAC_ASSERT(is_ex_of_type(self, indexed));
	GINAC_ASSERT(is_ex_of_type(self.op(0), matrix));
	GINAC_ASSERT(is_ex_of_type(other, indexed));
	GINAC_ASSERT(self.nops() == 2 || self.nops() == 3);

	// Only add two matrices
	if (is_ex_of_type(other.op(0), matrix)) {
		GINAC_ASSERT(other.nops() == 2 || other.nops() == 3);

		const matrix &self_matrix = ex_to<matrix>(self.op(0));
		const matrix &other_matrix = ex_to<matrix>(other.op(0));

		if (self.nops() == 2 && other.nops() == 2) { // vector + vector

			if (self_matrix.row == other_matrix.row)
				return indexed(self_matrix.add(other_matrix), self.op(1));
			else if (self_matrix.row == other_matrix.col)
				return indexed(self_matrix.add(other_matrix.transpose()), self.op(1));

		} else if (self.nops() == 3 && other.nops() == 3) { // matrix + matrix

			if (self.op(1).is_equal(other.op(1)) && self.op(2).is_equal(other.op(2)))
				return indexed(self_matrix.add(other_matrix), self.op(1), self.op(2));
			else if (self.op(1).is_equal(other.op(2)) && self.op(2).is_equal(other.op(1)))
				return indexed(self_matrix.add(other_matrix.transpose()), self.op(1), self.op(2));

		}
	}

	// Don't know what to do, return unevaluated sum
	return self + other;
}

/** Product of an indexed matrix with a number. */
ex matrix::scalar_mul_indexed(const ex & self, const numeric & other) const
{
	GINAC_ASSERT(is_ex_of_type(self, indexed));
	GINAC_ASSERT(is_ex_of_type(self.op(0), matrix));
	GINAC_ASSERT(self.nops() == 2 || self.nops() == 3);

	const matrix &self_matrix = ex_to<matrix>(self.op(0));

	if (self.nops() == 2)
		return indexed(self_matrix.mul(other), self.op(1));
	else // self.nops() == 3
		return indexed(self_matrix.mul(other), self.op(1), self.op(2));
}

/** Contraction of an indexed matrix with something else. */
bool matrix::contract_with(exvector::iterator self, exvector::iterator other, exvector & v) const
{
	GINAC_ASSERT(is_ex_of_type(*self, indexed));
	GINAC_ASSERT(is_ex_of_type(*other, indexed));
	GINAC_ASSERT(self->nops() == 2 || self->nops() == 3);
	GINAC_ASSERT(is_ex_of_type(self->op(0), matrix));

	// Only contract with other matrices
	if (!is_ex_of_type(other->op(0), matrix))
		return false;

	GINAC_ASSERT(other->nops() == 2 || other->nops() == 3);

	const matrix &self_matrix = ex_to<matrix>(self->op(0));
	const matrix &other_matrix = ex_to<matrix>(other->op(0));

	if (self->nops() == 2) {
		unsigned self_dim = (self_matrix.col == 1) ? self_matrix.row : self_matrix.col;

		if (other->nops() == 2) { // vector * vector (scalar product)
			unsigned other_dim = (other_matrix.col == 1) ? other_matrix.row : other_matrix.col;

			if (self_matrix.col == 1) {
				if (other_matrix.col == 1) {
					// Column vector * column vector, transpose first vector
					*self = self_matrix.transpose().mul(other_matrix)(0, 0);
				} else {
					// Column vector * row vector, swap factors
					*self = other_matrix.mul(self_matrix)(0, 0);
				}
			} else {
				if (other_matrix.col == 1) {
					// Row vector * column vector, perfect
					*self = self_matrix.mul(other_matrix)(0, 0);
				} else {
					// Row vector * row vector, transpose second vector
					*self = self_matrix.mul(other_matrix.transpose())(0, 0);
				}
			}
			*other = _ex1();
			return true;

		} else { // vector * matrix

			// B_i * A_ij = (B*A)_j (B is row vector)
			if (is_dummy_pair(self->op(1), other->op(1))) {
				if (self_matrix.row == 1)
					*self = indexed(self_matrix.mul(other_matrix), other->op(2));
				else
					*self = indexed(self_matrix.transpose().mul(other_matrix), other->op(2));
				*other = _ex1();
				return true;
			}

			// B_j * A_ij = (A*B)_i (B is column vector)
			if (is_dummy_pair(self->op(1), other->op(2))) {
				if (self_matrix.col == 1)
					*self = indexed(other_matrix.mul(self_matrix), other->op(1));
				else
					*self = indexed(other_matrix.mul(self_matrix.transpose()), other->op(1));
				*other = _ex1();
				return true;
			}
		}

	} else if (other->nops() == 3) { // matrix * matrix

		// A_ij * B_jk = (A*B)_ik
		if (is_dummy_pair(self->op(2), other->op(1))) {
			*self = indexed(self_matrix.mul(other_matrix), self->op(1), other->op(2));
			*other = _ex1();
			return true;
		}

		// A_ij * B_kj = (A*Btrans)_ik
		if (is_dummy_pair(self->op(2), other->op(2))) {
			*self = indexed(self_matrix.mul(other_matrix.transpose()), self->op(1), other->op(1));
			*other = _ex1();
			return true;
		}

		// A_ji * B_jk = (Atrans*B)_ik
		if (is_dummy_pair(self->op(1), other->op(1))) {
			*self = indexed(self_matrix.transpose().mul(other_matrix), self->op(2), other->op(2));
			*other = _ex1();
			return true;
		}

		// A_ji * B_kj = (B*A)_ki
		if (is_dummy_pair(self->op(1), other->op(2))) {
			*self = indexed(other_matrix.mul(self_matrix), other->op(1), self->op(2));
			*other = _ex1();
			return true;
		}
	}

	return false;
}


//////////
// non-virtual functions in this class
//////////

// public

/** Sum of matrices.
 *
 *  @exception logic_error (incompatible matrices) */
matrix matrix::add(const matrix & other) const
{
	if (col != other.col || row != other.row)
		throw std::logic_error("matrix::add(): incompatible matrices");
	
	exvector sum(this->m);
	exvector::iterator i;
	exvector::const_iterator ci;
	for (i=sum.begin(), ci=other.m.begin(); i!=sum.end(); ++i, ++ci)
		(*i) += (*ci);
	
	return matrix(row,col,sum);
}


/** Difference of matrices.
 *
 *  @exception logic_error (incompatible matrices) */
matrix matrix::sub(const matrix & other) const
{
	if (col != other.col || row != other.row)
		throw std::logic_error("matrix::sub(): incompatible matrices");
	
	exvector dif(this->m);
	exvector::iterator i;
	exvector::const_iterator ci;
	for (i=dif.begin(), ci=other.m.begin(); i!=dif.end(); ++i, ++ci)
		(*i) -= (*ci);
	
	return matrix(row,col,dif);
}


/** Product of matrices.
 *
 *  @exception logic_error (incompatible matrices) */
matrix matrix::mul(const matrix & other) const
{
	if (this->cols() != other.rows())
		throw std::logic_error("matrix::mul(): incompatible matrices");
	
	exvector prod(this->rows()*other.cols());
	
	for (unsigned r1=0; r1<this->rows(); ++r1) {
		for (unsigned c=0; c<this->cols(); ++c) {
			if (m[r1*col+c].is_zero())
				continue;
			for (unsigned r2=0; r2<other.cols(); ++r2)
				prod[r1*other.col+r2] += (m[r1*col+c] * other.m[c*other.col+r2]).expand();
		}
	}
	return matrix(row, other.col, prod);
}


/** Product of matrix and scalar. */
matrix matrix::mul(const numeric & other) const
{
	exvector prod(row * col);

	for (unsigned r=0; r<row; ++r)
		for (unsigned c=0; c<col; ++c)
			prod[r*col+c] = m[r*col+c] * other;

	return matrix(row, col, prod);
}


/** Product of matrix and scalar expression. */
matrix matrix::mul_scalar(const ex & other) const
{
	if (other.return_type() != return_types::commutative)
		throw std::runtime_error("matrix::mul_scalar(): non-commutative scalar");

	exvector prod(row * col);

	for (unsigned r=0; r<row; ++r)
		for (unsigned c=0; c<col; ++c)
			prod[r*col+c] = m[r*col+c] * other;

	return matrix(row, col, prod);
}


/** Power of a matrix.  Currently handles integer exponents only. */
matrix matrix::pow(const ex & expn) const
{
	if (col!=row)
		throw (std::logic_error("matrix::pow(): matrix not square"));
	
	if (is_ex_exactly_of_type(expn, numeric)) {
		// Integer cases are computed by successive multiplication, using the
		// obvious shortcut of storing temporaries, like A^4 == (A*A)*(A*A).
		if (expn.info(info_flags::integer)) {
			numeric k;
			matrix prod(row,col);
			if (expn.info(info_flags::negative)) {
				k = -ex_to<numeric>(expn);
				prod = this->inverse();
			} else {
				k = ex_to<numeric>(expn);
				prod = *this;
			}
			matrix result(row,col);
			for (unsigned r=0; r<row; ++r)
				result(r,r) = _ex1();
			numeric b(1);
			// this loop computes the representation of k in base 2 and
			// multiplies the factors whenever needed:
			while (b.compare(k)<=0) {
				b *= numeric(2);
				numeric r(mod(k,b));
				if (!r.is_zero()) {
					k -= r;
					result = result.mul(prod);
				}
				if (b.compare(k)<=0)
					prod = prod.mul(prod);
			}
			return result;
		}
	}
	throw (std::runtime_error("matrix::pow(): don't know how to handle exponent"));
}


/** operator() to access elements for reading.
 *
 *  @param ro row of element
 *  @param co column of element
 *  @exception range_error (index out of range) */
const ex & matrix::operator() (unsigned ro, unsigned co) const
{
	if (ro>=row || co>=col)
		throw (std::range_error("matrix::operator(): index out of range"));

	return m[ro*col+co];
}


/** operator() to access elements for writing.
 *
 *  @param ro row of element
 *  @param co column of element
 *  @exception range_error (index out of range) */
ex & matrix::operator() (unsigned ro, unsigned co)
{
	if (ro>=row || co>=col)
		throw (std::range_error("matrix::operator(): index out of range"));

	ensure_if_modifiable();
	return m[ro*col+co];
}


/** Transposed of an m x n matrix, producing a new n x m matrix object that
 *  represents the transposed. */
matrix matrix::transpose(void) const
{
	exvector trans(this->cols()*this->rows());
	
	for (unsigned r=0; r<this->cols(); ++r)
		for (unsigned c=0; c<this->rows(); ++c)
			trans[r*this->rows()+c] = m[c*this->cols()+r];
	
	return matrix(this->cols(),this->rows(),trans);
}

/** Determinant of square matrix.  This routine doesn't actually calculate the
 *  determinant, it only implements some heuristics about which algorithm to
 *  run.  If all the elements of the matrix are elements of an integral domain
 *  the determinant is also in that integral domain and the result is expanded
 *  only.  If one or more elements are from a quotient field the determinant is
 *  usually also in that quotient field and the result is normalized before it
 *  is returned.  This implies that the determinant of the symbolic 2x2 matrix
 *  [[a/(a-b),1],[b/(a-b),1]] is returned as unity.  (In this respect, it
 *  behaves like MapleV and unlike Mathematica.)
 *
 *  @param     algo allows to chose an algorithm
 *  @return    the determinant as a new expression
 *  @exception logic_error (matrix not square)
 *  @see       determinant_algo */
ex matrix::determinant(unsigned algo) const
{
	if (row!=col)
		throw (std::logic_error("matrix::determinant(): matrix not square"));
	GINAC_ASSERT(row*col==m.capacity());
	
	// Gather some statistical information about this matrix:
	bool numeric_flag = true;
	bool normal_flag = false;
	unsigned sparse_count = 0;  // counts non-zero elements
	for (exvector::const_iterator r=m.begin(); r!=m.end(); ++r) {
		lst srl;  // symbol replacement list
		ex rtest = (*r).to_rational(srl);
		if (!rtest.is_zero())
			++sparse_count;
		if (!rtest.info(info_flags::numeric))
			numeric_flag = false;
		if (!rtest.info(info_flags::crational_polynomial) &&
			 rtest.info(info_flags::rational_function))
			normal_flag = true;
	}
	
	// Here is the heuristics in case this routine has to decide:
	if (algo == determinant_algo::automatic) {
		// Minor expansion is generally a good guess:
		algo = determinant_algo::laplace;
		// Does anybody know when a matrix is really sparse?
		// Maybe <~row/2.236 nonzero elements average in a row?
		if (row>3 && 5*sparse_count<=row*col)
			algo = determinant_algo::bareiss;
		// Purely numeric matrix can be handled by Gauss elimination.
		// This overrides any prior decisions.
		if (numeric_flag)
			algo = determinant_algo::gauss;
	}
	
	// Trap the trivial case here, since some algorithms don't like it
	if (this->row==1) {
		// for consistency with non-trivial determinants...
		if (normal_flag)
			return m[0].normal();
		else
			return m[0].expand();
	}
	
	// Compute the determinant
	switch(algo) {
		case determinant_algo::gauss: {
			ex det = 1;
			matrix tmp(*this);
			int sign = tmp.gauss_elimination(true);
			for (unsigned d=0; d<row; ++d)
				det *= tmp.m[d*col+d];
			if (normal_flag)
				return (sign*det).normal();
			else
				return (sign*det).normal().expand();
		}
		case determinant_algo::bareiss: {
			matrix tmp(*this);
			int sign;
			sign = tmp.fraction_free_elimination(true);
			if (normal_flag)
				return (sign*tmp.m[row*col-1]).normal();
			else
				return (sign*tmp.m[row*col-1]).expand();
		}
		case determinant_algo::divfree: {
			matrix tmp(*this);
			int sign;
			sign = tmp.division_free_elimination(true);
			if (sign==0)
				return _ex0();
			ex det = tmp.m[row*col-1];
			// factor out accumulated bogus slag
			for (unsigned d=0; d<row-2; ++d)
				for (unsigned j=0; j<row-d-2; ++j)
					det = (det/tmp.m[d*col+d]).normal();
			return (sign*det);
		}
		case determinant_algo::laplace:
		default: {
			// This is the minor expansion scheme.  We always develop such
			// that the smallest minors (i.e, the trivial 1x1 ones) are on the
			// rightmost column.  For this to be efficient it turns out that
			// the emptiest columns (i.e. the ones with most zeros) should be
			// the ones on the right hand side.  Therefore we presort the
			// columns of the matrix:
			typedef std::pair<unsigned,unsigned> uintpair;
			std::vector<uintpair> c_zeros;  // number of zeros in column
			for (unsigned c=0; c<col; ++c) {
				unsigned acc = 0;
				for (unsigned r=0; r<row; ++r)
					if (m[r*col+c].is_zero())
						++acc;
				c_zeros.push_back(uintpair(acc,c));
			}
			sort(c_zeros.begin(),c_zeros.end());
			std::vector<unsigned> pre_sort;
			for (std::vector<uintpair>::iterator i=c_zeros.begin(); i!=c_zeros.end(); ++i)
				pre_sort.push_back(i->second);
			std::vector<unsigned> pre_sort_test(pre_sort); // permutation_sign() modifies the vector so we make a copy here
			int sign = permutation_sign(pre_sort_test.begin(), pre_sort_test.end());
			exvector result(row*col);  // represents sorted matrix
			unsigned c = 0;
			for (std::vector<unsigned>::iterator i=pre_sort.begin();
				 i!=pre_sort.end();
				 ++i,++c) {
				for (unsigned r=0; r<row; ++r)
					result[r*col+c] = m[r*col+(*i)];
			}
			
			if (normal_flag)
				return (sign*matrix(row,col,result).determinant_minor()).normal();
			else
				return sign*matrix(row,col,result).determinant_minor();
		}
	}
}


/** Trace of a matrix.  The result is normalized if it is in some quotient
 *  field and expanded only otherwise.  This implies that the trace of the
 *  symbolic 2x2 matrix [[a/(a-b),x],[y,b/(b-a)]] is recognized to be unity.
 *
 *  @return    the sum of diagonal elements
 *  @exception logic_error (matrix not square) */
ex matrix::trace(void) const
{
	if (row != col)
		throw (std::logic_error("matrix::trace(): matrix not square"));
	
	ex tr;
	for (unsigned r=0; r<col; ++r)
		tr += m[r*col+r];
	
	if (tr.info(info_flags::rational_function) &&
		!tr.info(info_flags::crational_polynomial))
		return tr.normal();
	else
		return tr.expand();
}


/** Characteristic Polynomial.  Following mathematica notation the
 *  characteristic polynomial of a matrix M is defined as the determiant of
 *  (M - lambda * 1) where 1 stands for the unit matrix of the same dimension
 *  as M.  Note that some CASs define it with a sign inside the determinant
 *  which gives rise to an overall sign if the dimension is odd.  This method
 *  returns the characteristic polynomial collected in powers of lambda as a
 *  new expression.
 *
 *  @return    characteristic polynomial as new expression
 *  @exception logic_error (matrix not square)
 *  @see       matrix::determinant() */
ex matrix::charpoly(const symbol & lambda) const
{
	if (row != col)
		throw (std::logic_error("matrix::charpoly(): matrix not square"));
	
	bool numeric_flag = true;
	for (exvector::const_iterator r=m.begin(); r!=m.end(); ++r) {
		if (!(*r).info(info_flags::numeric)) {
			numeric_flag = false;
		}
	}
	
	// The pure numeric case is traditionally rather common.  Hence, it is
	// trapped and we use Leverrier's algorithm which goes as row^3 for
	// every coefficient.  The expensive part is the matrix multiplication.
	if (numeric_flag) {
		matrix B(*this);
		ex c = B.trace();
		ex poly = power(lambda,row)-c*power(lambda,row-1);
		for (unsigned i=1; i<row; ++i) {
			for (unsigned j=0; j<row; ++j)
				B.m[j*col+j] -= c;
			B = this->mul(B);
			c = B.trace()/ex(i+1);
			poly -= c*power(lambda,row-i-1);
		}
		if (row%2)
			return -poly;
		else
			return poly;
	}
	
	matrix M(*this);
	for (unsigned r=0; r<col; ++r)
		M.m[r*col+r] -= lambda;
	
	return M.determinant().collect(lambda);
}


/** Inverse of this matrix.
 *
 *  @return    the inverted matrix
 *  @exception logic_error (matrix not square)
 *  @exception runtime_error (singular matrix) */
matrix matrix::inverse(void) const
{
	if (row != col)
		throw (std::logic_error("matrix::inverse(): matrix not square"));
	
	// This routine actually doesn't do anything fancy at all.  We compute the
	// inverse of the matrix A by solving the system A * A^{-1} == Id.
	
	// First populate the identity matrix supposed to become the right hand side.
	matrix identity(row,col);
	for (unsigned i=0; i<row; ++i)
		identity(i,i) = _ex1();
	
	// Populate a dummy matrix of variables, just because of compatibility with
	// matrix::solve() which wants this (for compatibility with under-determined
	// systems of equations).
	matrix vars(row,col);
	for (unsigned r=0; r<row; ++r)
		for (unsigned c=0; c<col; ++c)
			vars(r,c) = symbol();
	
	matrix sol(row,col);
	try {
		sol = this->solve(vars,identity);
	} catch (const std::runtime_error & e) {
	    if (e.what()==std::string("matrix::solve(): inconsistent linear system"))
			throw (std::runtime_error("matrix::inverse(): singular matrix"));
		else
			throw;
	}
	return sol;
}


/** Solve a linear system consisting of a m x n matrix and a m x p right hand
 *  side by applying an elimination scheme to the augmented matrix.
 *
 *  @param vars n x p matrix, all elements must be symbols 
 *  @param rhs m x p matrix
 *  @return n x p solution matrix
 *  @exception logic_error (incompatible matrices)
 *  @exception invalid_argument (1st argument must be matrix of symbols)
 *  @exception runtime_error (inconsistent linear system)
 *  @see       solve_algo */
matrix matrix::solve(const matrix & vars,
					 const matrix & rhs,
					 unsigned algo) const
{
	const unsigned m = this->rows();
	const unsigned n = this->cols();
	const unsigned p = rhs.cols();
	
	// syntax checks    
	if ((rhs.rows() != m) || (vars.rows() != n) || (vars.col != p))
		throw (std::logic_error("matrix::solve(): incompatible matrices"));
	for (unsigned ro=0; ro<n; ++ro)
		for (unsigned co=0; co<p; ++co)
			if (!vars(ro,co).info(info_flags::symbol))
				throw (std::invalid_argument("matrix::solve(): 1st argument must be matrix of symbols"));
	
	// build the augmented matrix of *this with rhs attached to the right
	matrix aug(m,n+p);
	for (unsigned r=0; r<m; ++r) {
		for (unsigned c=0; c<n; ++c)
			aug.m[r*(n+p)+c] = this->m[r*n+c];
		for (unsigned c=0; c<p; ++c)
			aug.m[r*(n+p)+c+n] = rhs.m[r*p+c];
	}
	
	// Gather some statistical information about the augmented matrix:
	bool numeric_flag = true;
	for (exvector::const_iterator r=aug.m.begin(); r!=aug.m.end(); ++r) {
		if (!(*r).info(info_flags::numeric))
			numeric_flag = false;
	}
	
	// Here is the heuristics in case this routine has to decide:
	if (algo == solve_algo::automatic) {
		// Bareiss (fraction-free) elimination is generally a good guess:
		algo = solve_algo::bareiss;
		// For m<3, Bareiss elimination is equivalent to division free
		// elimination but has more logistic overhead
		if (m<3)
			algo = solve_algo::divfree;
		// This overrides any prior decisions.
		if (numeric_flag)
			algo = solve_algo::gauss;
	}
	
	// Eliminate the augmented matrix:
	switch(algo) {
		case solve_algo::gauss:
			aug.gauss_elimination();
			break;
		case solve_algo::divfree:
			aug.division_free_elimination();
			break;
		case solve_algo::bareiss:
		default:
			aug.fraction_free_elimination();
	}
	
	// assemble the solution matrix:
	matrix sol(n,p);
	for (unsigned co=0; co<p; ++co) {
		unsigned last_assigned_sol = n+1;
		for (int r=m-1; r>=0; --r) {
			unsigned fnz = 1;    // first non-zero in row
			while ((fnz<=n) && (aug.m[r*(n+p)+(fnz-1)].is_zero()))
				++fnz;
			if (fnz>n) {
				// row consists only of zeros, corresponding rhs must be 0, too
				if (!aug.m[r*(n+p)+n+co].is_zero()) {
					throw (std::runtime_error("matrix::solve(): inconsistent linear system"));
				}
			} else {
				// assign solutions for vars between fnz+1 and
				// last_assigned_sol-1: free parameters
				for (unsigned c=fnz; c<last_assigned_sol-1; ++c)
					sol(c,co) = vars.m[c*p+co];
				ex e = aug.m[r*(n+p)+n+co];
				for (unsigned c=fnz; c<n; ++c)
					e -= aug.m[r*(n+p)+c]*sol.m[c*p+co];
				sol(fnz-1,co) = (e/(aug.m[r*(n+p)+(fnz-1)])).normal();
				last_assigned_sol = fnz;
			}
		}
		// assign solutions for vars between 1 and
		// last_assigned_sol-1: free parameters
		for (unsigned ro=0; ro<last_assigned_sol-1; ++ro)
			sol(ro,co) = vars(ro,co);
	}
	
	return sol;
}


// protected

/** Recursive determinant for small matrices having at least one symbolic
 *  entry.  The basic algorithm, known as Laplace-expansion, is enhanced by
 *  some bookkeeping to avoid calculation of the same submatrices ("minors")
 *  more than once.  According to W.M.Gentleman and S.C.Johnson this algorithm
 *  is better than elimination schemes for matrices of sparse multivariate
 *  polynomials and also for matrices of dense univariate polynomials if the
 *  matrix' dimesion is larger than 7.
 *
 *  @return the determinant as a new expression (in expanded form)
 *  @see matrix::determinant() */
ex matrix::determinant_minor(void) const
{
	// for small matrices the algorithm does not make any sense:
	const unsigned n = this->cols();
	if (n==1)
		return m[0].expand();
	if (n==2)
		return (m[0]*m[3]-m[2]*m[1]).expand();
	if (n==3)
		return (m[0]*m[4]*m[8]-m[0]*m[5]*m[7]-
		        m[1]*m[3]*m[8]+m[2]*m[3]*m[7]+
		        m[1]*m[5]*m[6]-m[2]*m[4]*m[6]).expand();
	
	// This algorithm can best be understood by looking at a naive
	// implementation of Laplace-expansion, like this one:
	// ex det;
	// matrix minorM(this->rows()-1,this->cols()-1);
	// for (unsigned r1=0; r1<this->rows(); ++r1) {
	//     // shortcut if element(r1,0) vanishes
	//     if (m[r1*col].is_zero())
	//         continue;
	//     // assemble the minor matrix
	//     for (unsigned r=0; r<minorM.rows(); ++r) {
	//         for (unsigned c=0; c<minorM.cols(); ++c) {
	//             if (r<r1)
	//                 minorM(r,c) = m[r*col+c+1];
	//             else
	//                 minorM(r,c) = m[(r+1)*col+c+1];
	//         }
	//     }
	//     // recurse down and care for sign:
	//     if (r1%2)
	//         det -= m[r1*col] * minorM.determinant_minor();
	//     else
	//         det += m[r1*col] * minorM.determinant_minor();
	// }
	// return det.expand();
	// What happens is that while proceeding down many of the minors are
	// computed more than once.  In particular, there are binomial(n,k)
	// kxk minors and each one is computed factorial(n-k) times.  Therefore
	// it is reasonable to store the results of the minors.  We proceed from
	// right to left.  At each column c we only need to retrieve the minors
	// calculated in step c-1.  We therefore only have to store at most 
	// 2*binomial(n,n/2) minors.
	
	// Unique flipper counter for partitioning into minors
	std::vector<unsigned> Pkey;
	Pkey.reserve(n);
	// key for minor determinant (a subpartition of Pkey)
	std::vector<unsigned> Mkey;
	Mkey.reserve(n-1);
	// we store our subminors in maps, keys being the rows they arise from
	typedef std::map<std::vector<unsigned>,class ex> Rmap;
	typedef std::map<std::vector<unsigned>,class ex>::value_type Rmap_value;
	Rmap A;
	Rmap B;
	ex det;
	// initialize A with last column:
	for (unsigned r=0; r<n; ++r) {
		Pkey.erase(Pkey.begin(),Pkey.end());
		Pkey.push_back(r);
		A.insert(Rmap_value(Pkey,m[n*(r+1)-1]));
	}
	// proceed from right to left through matrix
	for (int c=n-2; c>=0; --c) {
		Pkey.erase(Pkey.begin(),Pkey.end());  // don't change capacity
		Mkey.erase(Mkey.begin(),Mkey.end());
		for (unsigned i=0; i<n-c; ++i)
			Pkey.push_back(i);
		unsigned fc = 0;  // controls logic for our strange flipper counter
		do {
			det = _ex0();
			for (unsigned r=0; r<n-c; ++r) {
				// maybe there is nothing to do?
				if (m[Pkey[r]*n+c].is_zero())
					continue;
				// create the sorted key for all possible minors
				Mkey.erase(Mkey.begin(),Mkey.end());
				for (unsigned i=0; i<n-c; ++i)
					if (i!=r)
						Mkey.push_back(Pkey[i]);
				// Fetch the minors and compute the new determinant
				if (r%2)
					det -= m[Pkey[r]*n+c]*A[Mkey];
				else
					det += m[Pkey[r]*n+c]*A[Mkey];
			}
			// prevent build-up of deep nesting of expressions saves time:
			det = det.expand();
			// store the new determinant at its place in B:
			if (!det.is_zero())
				B.insert(Rmap_value(Pkey,det));
			// increment our strange flipper counter
			for (fc=n-c; fc>0; --fc) {
				++Pkey[fc-1];
				if (Pkey[fc-1]<fc+c)
					break;
			}
			if (fc<n-c && fc>0)
				for (unsigned j=fc; j<n-c; ++j)
					Pkey[j] = Pkey[j-1]+1;
		} while(fc);
		// next column, so change the role of A and B:
		A = B;
		B.clear();
	}
	
	return det;
}


/** Perform the steps of an ordinary Gaussian elimination to bring the m x n
 *  matrix into an upper echelon form.  The algorithm is ok for matrices
 *  with numeric coefficients but quite unsuited for symbolic matrices.
 *
 *  @param det may be set to true to save a lot of space if one is only
 *  interested in the diagonal elements (i.e. for calculating determinants).
 *  The others are set to zero in this case.
 *  @return sign is 1 if an even number of rows was swapped, -1 if an odd
 *  number of rows was swapped and 0 if the matrix is singular. */
int matrix::gauss_elimination(const bool det)
{
	ensure_if_modifiable();
	const unsigned m = this->rows();
	const unsigned n = this->cols();
	GINAC_ASSERT(!det || n==m);
	int sign = 1;
	
	unsigned r0 = 0;
	for (unsigned r1=0; (r1<n-1)&&(r0<m-1); ++r1) {
		int indx = pivot(r0, r1, true);
		if (indx == -1) {
			sign = 0;
			if (det)
				return 0;  // leaves *this in a messy state
		}
		if (indx>=0) {
			if (indx > 0)
				sign = -sign;
			for (unsigned r2=r0+1; r2<m; ++r2) {
				if (!this->m[r2*n+r1].is_zero()) {
					// yes, there is something to do in this row
					ex piv = this->m[r2*n+r1] / this->m[r0*n+r1];
					for (unsigned c=r1+1; c<n; ++c) {
						this->m[r2*n+c] -= piv * this->m[r0*n+c];
						if (!this->m[r2*n+c].info(info_flags::numeric))
							this->m[r2*n+c] = this->m[r2*n+c].normal();
					}
				}
				// fill up left hand side with zeros
				for (unsigned c=0; c<=r1; ++c)
					this->m[r2*n+c] = _ex0();
			}
			if (det) {
				// save space by deleting no longer needed elements
				for (unsigned c=r0+1; c<n; ++c)
					this->m[r0*n+c] = _ex0();
			}
			++r0;
		}
	}
	
	return sign;
}


/** Perform the steps of division free elimination to bring the m x n matrix
 *  into an upper echelon form.
 *
 *  @param det may be set to true to save a lot of space if one is only
 *  interested in the diagonal elements (i.e. for calculating determinants).
 *  The others are set to zero in this case.
 *  @return sign is 1 if an even number of rows was swapped, -1 if an odd
 *  number of rows was swapped and 0 if the matrix is singular. */
int matrix::division_free_elimination(const bool det)
{
	ensure_if_modifiable();
	const unsigned m = this->rows();
	const unsigned n = this->cols();
	GINAC_ASSERT(!det || n==m);
	int sign = 1;
	
	unsigned r0 = 0;
	for (unsigned r1=0; (r1<n-1)&&(r0<m-1); ++r1) {
		int indx = pivot(r0, r1, true);
		if (indx==-1) {
			sign = 0;
			if (det)
				return 0;  // leaves *this in a messy state
		}
		if (indx>=0) {
			if (indx>0)
				sign = -sign;
			for (unsigned r2=r0+1; r2<m; ++r2) {
				for (unsigned c=r1+1; c<n; ++c)
					this->m[r2*n+c] = (this->m[r0*n+r1]*this->m[r2*n+c] - this->m[r2*n+r1]*this->m[r0*n+c]).expand();
				// fill up left hand side with zeros
				for (unsigned c=0; c<=r1; ++c)
					this->m[r2*n+c] = _ex0();
			}
			if (det) {
				// save space by deleting no longer needed elements
				for (unsigned c=r0+1; c<n; ++c)
					this->m[r0*n+c] = _ex0();
			}
			++r0;
		}
	}
	
	return sign;
}


/** Perform the steps of Bareiss' one-step fraction free elimination to bring
 *  the matrix into an upper echelon form.  Fraction free elimination means
 *  that divide is used straightforwardly, without computing GCDs first.  This
 *  is possible, since we know the divisor at each step.
 *  
 *  @param det may be set to true to save a lot of space if one is only
 *  interested in the last element (i.e. for calculating determinants). The
 *  others are set to zero in this case.
 *  @return sign is 1 if an even number of rows was swapped, -1 if an odd
 *  number of rows was swapped and 0 if the matrix is singular. */
int matrix::fraction_free_elimination(const bool det)
{
	// Method:
	// (single-step fraction free elimination scheme, already known to Jordan)
	//
	// Usual division-free elimination sets m[0](r,c) = m(r,c) and then sets
	//     m[k+1](r,c) = m[k](k,k) * m[k](r,c) - m[k](r,k) * m[k](k,c).
	//
	// Bareiss (fraction-free) elimination in addition divides that element
	// by m[k-1](k-1,k-1) for k>1, where it can be shown by means of the
	// Sylvester determinant that this really divides m[k+1](r,c).
	//
	// We also allow rational functions where the original prove still holds.
	// However, we must care for numerator and denominator separately and
	// "manually" work in the integral domains because of subtle cancellations
	// (see below).  This blows up the bookkeeping a bit and the formula has
	// to be modified to expand like this (N{x} stands for numerator of x,
	// D{x} for denominator of x):
	//     N{m[k+1](r,c)} = N{m[k](k,k)}*N{m[k](r,c)}*D{m[k](r,k)}*D{m[k](k,c)}
	//                     -N{m[k](r,k)}*N{m[k](k,c)}*D{m[k](k,k)}*D{m[k](r,c)}
	//     D{m[k+1](r,c)} = D{m[k](k,k)}*D{m[k](r,c)}*D{m[k](r,k)}*D{m[k](k,c)}
	// where for k>1 we now divide N{m[k+1](r,c)} by
	//     N{m[k-1](k-1,k-1)}
	// and D{m[k+1](r,c)} by
	//     D{m[k-1](k-1,k-1)}.
	
	ensure_if_modifiable();
	const unsigned m = this->rows();
	const unsigned n = this->cols();
	GINAC_ASSERT(!det || n==m);
	int sign = 1;
	if (m==1)
		return 1;
	ex divisor_n = 1;
	ex divisor_d = 1;
	ex dividend_n;
	ex dividend_d;
	
	// We populate temporary matrices to subsequently operate on.  There is
	// one holding numerators and another holding denominators of entries.
	// This is a must since the evaluator (or even earlier mul's constructor)
	// might cancel some trivial element which causes divide() to fail.  The
	// elements are normalized first (yes, even though this algorithm doesn't
	// need GCDs) since the elements of *this might be unnormalized, which
	// makes things more complicated than they need to be.
	matrix tmp_n(*this);
	matrix tmp_d(m,n);  // for denominators, if needed
	lst srl;  // symbol replacement list
	exvector::iterator it = this->m.begin();
	exvector::iterator tmp_n_it = tmp_n.m.begin();
	exvector::iterator tmp_d_it = tmp_d.m.begin();
	for (; it!= this->m.end(); ++it, ++tmp_n_it, ++tmp_d_it) {
		(*tmp_n_it) = (*it).normal().to_rational(srl);
		(*tmp_d_it) = (*tmp_n_it).denom();
		(*tmp_n_it) = (*tmp_n_it).numer();
	}
	
	unsigned r0 = 0;
	for (unsigned r1=0; (r1<n-1)&&(r0<m-1); ++r1) {
		int indx = tmp_n.pivot(r0, r1, true);
		if (indx==-1) {
			sign = 0;
			if (det)
				return 0;
		}
		if (indx>=0) {
			if (indx>0) {
				sign = -sign;
				// tmp_n's rows r0 and indx were swapped, do the same in tmp_d:
				for (unsigned c=r1; c<n; ++c)
					tmp_d.m[n*indx+c].swap(tmp_d.m[n*r0+c]);
			}
			for (unsigned r2=r0+1; r2<m; ++r2) {
				for (unsigned c=r1+1; c<n; ++c) {
					dividend_n = (tmp_n.m[r0*n+r1]*tmp_n.m[r2*n+c]*
					              tmp_d.m[r2*n+r1]*tmp_d.m[r0*n+c]
					             -tmp_n.m[r2*n+r1]*tmp_n.m[r0*n+c]*
					              tmp_d.m[r0*n+r1]*tmp_d.m[r2*n+c]).expand();
					dividend_d = (tmp_d.m[r2*n+r1]*tmp_d.m[r0*n+c]*
					              tmp_d.m[r0*n+r1]*tmp_d.m[r2*n+c]).expand();
					bool check = divide(dividend_n, divisor_n,
					                    tmp_n.m[r2*n+c], true);
					check &= divide(dividend_d, divisor_d,
					                tmp_d.m[r2*n+c], true);
					GINAC_ASSERT(check);
				}
				// fill up left hand side with zeros
				for (unsigned c=0; c<=r1; ++c)
					tmp_n.m[r2*n+c] = _ex0();
			}
			if ((r1<n-1)&&(r0<m-1)) {
				// compute next iteration's divisor
				divisor_n = tmp_n.m[r0*n+r1].expand();
				divisor_d = tmp_d.m[r0*n+r1].expand();
				if (det) {
					// save space by deleting no longer needed elements
					for (unsigned c=0; c<n; ++c) {
						tmp_n.m[r0*n+c] = _ex0();
						tmp_d.m[r0*n+c] = _ex1();
					}
				}
			}
			++r0;
		}
	}
	// repopulate *this matrix:
	it = this->m.begin();
	tmp_n_it = tmp_n.m.begin();
	tmp_d_it = tmp_d.m.begin();
	for (; it!= this->m.end(); ++it, ++tmp_n_it, ++tmp_d_it)
		(*it) = ((*tmp_n_it)/(*tmp_d_it)).subs(srl);
	
	return sign;
}


/** Partial pivoting method for matrix elimination schemes.
 *  Usual pivoting (symbolic==false) returns the index to the element with the
 *  largest absolute value in column ro and swaps the current row with the one
 *  where the element was found.  With (symbolic==true) it does the same thing
 *  with the first non-zero element.
 *
 *  @param ro is the row from where to begin
 *  @param co is the column to be inspected
 *  @param symbolic signal if we want the first non-zero element to be pivoted
 *  (true) or the one with the largest absolute value (false).
 *  @return 0 if no interchange occured, -1 if all are zero (usually signaling
 *  a degeneracy) and positive integer k means that rows ro and k were swapped.
 */
int matrix::pivot(unsigned ro, unsigned co, bool symbolic)
{
	unsigned k = ro;
	if (symbolic) {
		// search first non-zero element in column co beginning at row ro
		while ((k<row) && (this->m[k*col+co].expand().is_zero()))
			++k;
	} else {
		// search largest element in column co beginning at row ro
		GINAC_ASSERT(is_ex_of_type(this->m[k*col+co],numeric));
		unsigned kmax = k+1;
		numeric mmax = abs(ex_to<numeric>(m[kmax*col+co]));
		while (kmax<row) {
			GINAC_ASSERT(is_ex_of_type(this->m[kmax*col+co],numeric));
			numeric tmp = ex_to<numeric>(this->m[kmax*col+co]);
			if (abs(tmp) > mmax) {
				mmax = tmp;
				k = kmax;
			}
			++kmax;
		}
		if (!mmax.is_zero())
			k = kmax;
	}
	if (k==row)
		// all elements in column co below row ro vanish
		return -1;
	if (k==ro)
		// matrix needs no pivoting
		return 0;
	// matrix needs pivoting, so swap rows k and ro
	ensure_if_modifiable();
	for (unsigned c=0; c<col; ++c)
		this->m[k*col+c].swap(this->m[ro*col+c]);
	
	return k;
}

ex lst_to_matrix(const lst & l)
{
	// Find number of rows and columns
	unsigned rows = l.nops(), cols = 0, i, j;
	for (i=0; i<rows; i++)
		if (l.op(i).nops() > cols)
			cols = l.op(i).nops();

	// Allocate and fill matrix
	matrix &m = *new matrix(rows, cols);
	m.setflag(status_flags::dynallocated);
	for (i=0; i<rows; i++)
		for (j=0; j<cols; j++)
			if (l.op(i).nops() > j)
				m(i, j) = l.op(i).op(j);
			else
				m(i, j) = _ex0();
	return m;
}

ex diag_matrix(const lst & l)
{
	unsigned dim = l.nops();

	matrix &m = *new matrix(dim, dim);
	m.setflag(status_flags::dynallocated);
	for (unsigned i=0; i<dim; i++)
		m(i, i) = l.op(i);

	return m;
}

} // namespace GiNaC
