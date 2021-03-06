//File: $Id$
// Author: John Wu <John.Wu at ACM.org>
//         Lawrence Berkeley National Laboratory
// Copyright 2000-2009 the Regents of the University of California
#ifndef IBIS_IRELIC_H
#define IBIS_IRELIC_H
///@file
/// Define ibis::relic and its derived classes
///@verbatim
/// relic -> slice, fade, bylt(pack), zona (zone), fuzz
/// fade -> sbiad, sapid
///@endverbatim
///
#if defined(_WIN32) && defined(_MSC_VER)
#pragma warning(disable:4786)	// some identifier longer than 256 characters
#endif
#include "index.h"

/// The basic bitmap index.  It generates one bitmap for each distinct
/// value.
class ibis::relic : public ibis::index {
public:
    virtual ~relic() {clear();};
    relic(const ibis::column* c, const char* f = 0);
    relic(const ibis::column* c, uint32_t popu, uint32_t ntpl=0);
    relic(const ibis::column* c, uint32_t card, array_t<uint32_t>& ints);
    relic(const ibis::column* c, ibis::fileManager::storage* st,
	  size_t start = 8);

    virtual void print(std::ostream& out) const;
    virtual int  write(const char* dt) const;
    virtual int  read(const char* idxfile);
    virtual int  read(ibis::fileManager::storage* st);
    virtual long append(const char* dt, const char* df, uint32_t nnew);

    using ibis::index::estimate;
    using ibis::index::estimateCost;
    virtual long evaluate(const ibis::qContinuousRange& expr,
			  ibis::bitvector& hits) const;
    virtual long evaluate(const ibis::qDiscreteRange& expr,
			  ibis::bitvector& hits) const;

    virtual void estimate(const ibis::qContinuousRange& expr,
			  ibis::bitvector& lower,
			  ibis::bitvector& upper) const {
	(void) evaluate(expr, lower);
	upper.clear();
    }
    virtual uint32_t estimate(const ibis::qContinuousRange& expr) const;
    /// This class and its derived classes should produce exact answers,
    /// therefore no undecidable rows.
    virtual float undecidable(const ibis::qContinuousRange& expr,
			      ibis::bitvector& iffy) const {
	iffy.clear();
	return 0.0;
    }
    virtual void estimate(const ibis::qDiscreteRange& expr,
			  ibis::bitvector& lower,
			  ibis::bitvector& upper) const {
	evaluate(expr, lower);
	upper.clear();
    }
    virtual uint32_t estimate(const ibis::qDiscreteRange& expr) const;
    virtual float undecidable(const ibis::qDiscreteRange& expr,
			      ibis::bitvector& iffy) const {
	iffy.clear();
	return 0.0;
    }

    virtual double estimateCost(const ibis::qContinuousRange& expr) const;
    virtual double estimateCost(const ibis::qDiscreteRange& expr) const;

    /// Estimate the pairs for the range join operator.  Only records that
    /// are masked are evaluated.
    virtual void estimate(const ibis::relic& idx2,
			  const ibis::rangeJoin& expr,
			  const ibis::bitvector& mask,
			  ibis::bitvector64& lower,
			  ibis::bitvector64& upper) const;
    virtual void estimate(const ibis::relic& idx2,
			  const ibis::rangeJoin& expr,
			  const ibis::bitvector& mask,
			  const ibis::qRange* const range1,
			  const ibis::qRange* const range2,
			  ibis::bitvector64& lower,
			  ibis::bitvector64& upper) const;
    /// Estimate an upper bound for the number of pairs produced from
    /// marked records.
    virtual int64_t estimate(const ibis::relic& idx2,
			     const ibis::rangeJoin& expr,
			     const ibis::bitvector& mask) const;
    virtual int64_t estimate(const ibis::relic& idx2,
			     const ibis::rangeJoin& expr,
			     const ibis::bitvector& mask,
			     const ibis::qRange* const range1,
			     const ibis::qRange* const range2) const;

    virtual INDEX_TYPE type() const {return RELIC;}
    virtual const char* name() const {return "basic";}
    // bin boundaries and counts of each bin
    virtual void binBoundaries(std::vector<double>& b) const;
    virtual void binWeights(std::vector<uint32_t>& b) const;

    virtual long getCumulativeDistribution(std::vector<double>& bds,
					   std::vector<uint32_t>& cts) const;
    virtual long getDistribution(std::vector<double>& bds,
				 std::vector<uint32_t>& cts) const;
    virtual double getMin() const {return (vals.empty()?DBL_MAX:vals[0]);}
    virtual double getMax() const {return (vals.empty()?-DBL_MAX:vals.back());}
    virtual double getSum() const;

    virtual void speedTest(std::ostream& out) const;
    long append(const ibis::relic& tail);
    long append(const array_t<uint32_t>& ind);
    array_t<uint32_t>* keys(const ibis::bitvector& mask) const;

protected:
    // protected member variables
    array_t<double> vals;

    int write32(int fdes) const;
    int write64(int fdes) const;
    // protected member functions
    uint32_t locate(const double& val) const;
    void     locate(const ibis::qContinuousRange& expr,
		    uint32_t& hit0, uint32_t& hit1) const;

    // a dummy constructor
    relic() : ibis::index() {}
    // free current resources, re-initialized all member variables
    virtual void clear();
    virtual double computeSum() const;
    virtual size_t getSerialSize() const throw();

    /// Construct an index from in-memory values.  The type @c E is
    /// intended to be element types supported in column.h.
    template <typename E>
    void construct(const array_t<E>& arr);
    /// Construct a new index in memory.
    void construct(const char* f = 0);

private:
    // private member functions
    int64_t equiJoin(const ibis::relic& idx2, const ibis::bitvector& mask,
		     ibis::bitvector64& hits) const;
    int64_t rangeJoin(const ibis::relic& idx2, const ibis::bitvector& mask,
		      const double& delta, ibis::bitvector64& hits) const;
    int64_t compJoin(const ibis::relic& idx2, const ibis::bitvector& mask,
		     const ibis::math::term& delta,
		     ibis::bitvector64& hits) const;

    int64_t equiJoin(const ibis::relic& idx2,
		     const ibis::bitvector& mask) const;
    int64_t rangeJoin(const ibis::relic& idx2, const ibis::bitvector& mask,
		      const double& delta) const;
    int64_t compJoin(const ibis::relic& idx2, const ibis::bitvector& mask,
		     const ibis::math::term& delta) const;

    int64_t equiJoin(const ibis::relic& idx2, const ibis::bitvector& mask,
		     const ibis::qRange* const range1,
		     const ibis::qRange* const range2,
		     ibis::bitvector64& hits) const;
    int64_t rangeJoin(const ibis::relic& idx2, const ibis::bitvector& mask,
		      const ibis::qRange* const range1,
		      const ibis::qRange* const range2,
		      const double& delta, ibis::bitvector64& hits) const;
    /// Can not make good use of range restrictions when the distance
    /// function in the join expression is an arbitrary function.
    int64_t compJoin(const ibis::relic& idx2, const ibis::bitvector& mask,
		     const ibis::qRange* const range1,
		     const ibis::qRange* const range2,
		     const ibis::math::term& delta,
		     ibis::bitvector64& hits) const {
	return compJoin(idx2, mask, delta, hits);
    }

    int64_t equiJoin(const ibis::relic& idx2,
		     const ibis::bitvector& mask,
		     const ibis::qRange* const range1,
		     const ibis::qRange* const range2) const;
    int64_t rangeJoin(const ibis::relic& idx2, const ibis::bitvector& mask,
		      const ibis::qRange* const range1,
		      const ibis::qRange* const range2,
		      const double& delta) const;
    /// Can not make good use of range restrictions when the distance
    /// function in the join expression is an arbitrary function.
    int64_t compJoin(const ibis::relic& idx2, const ibis::bitvector& mask,
		     const ibis::qRange* const range1,
		     const ibis::qRange* const range2,
		     const ibis::math::term& delta) const {
	return compJoin(idx2, mask, delta);
    }

    relic(const relic&);
    relic& operator=(const relic&);
}; // ibis::relic

/// The bit-sliced index (O'Neil).  It used the binary encoding.
class ibis::slice : public ibis::relic {
public:
    virtual ~slice() {clear();};
    slice(const ibis::column* c = 0, const char* f = 0);
    slice(const ibis::column* c, ibis::fileManager::storage* st,
	  size_t start = 8);

    virtual int write(const char* dt) const;
    virtual void print(std::ostream& out) const;
    virtual int read(const char* idxfile);
    virtual int read(ibis::fileManager::storage* st);

    virtual long append(const char* dt, const char* df, uint32_t nnew);

    using ibis::relic::estimate;
    using ibis::relic::estimateCost;
    virtual long evaluate(const ibis::qContinuousRange& expr,
			  ibis::bitvector& hits) const;
    virtual long evaluate(const ibis::qDiscreteRange& expr,
			  ibis::bitvector& hits) const;

    virtual void estimate(const ibis::qContinuousRange& expr,
			  ibis::bitvector& lower,
			  ibis::bitvector& upper) const;
    virtual uint32_t estimate(const ibis::qContinuousRange& expr) const;
    virtual INDEX_TYPE type() const {return SLICE;}
    virtual const char* name() const {return "bit-sliced";}
    // number of records in each bin
    virtual void binWeights(std::vector<uint32_t>& b) const;
    virtual double getSum() const;

    virtual void speedTest(std::ostream& out) const;
    virtual double estimateCost(const ibis::qContinuousRange& expr) const {
	double ret;
	if (offset64.size() > bits.size())
	    ret = offset64.back();
	else if (offset32.size() > bits.size())
	    ret = offset32.back();
	else
	    ret = 0.0;
	return ret;
    }
    virtual double estimateCost(const ibis::qDiscreteRange& expr) const {
	double ret;
	if (offset64.size() > bits.size())
	    ret = offset64.back();
	else if (offset32.size() > bits.size())
	    ret = offset32.back();
	else
	    ret = 0.0;
	return ret;
    }

protected:
    virtual void clear();
    virtual size_t getSerialSize() const throw();

private:
    // private member variables
    array_t<uint32_t> cnts; // the counts for each distinct value

    // private member functions
    void construct1(const char* f = 0); // uses more temporary storage
    void construct2(const char* f = 0); // passes through data twice
    void setBit(const uint32_t i, const double val);

    int write32(int fdes) const;
    int write64(int fdes) const;
    void evalGE(ibis::bitvector& res, uint32_t b) const;
    void evalEQ(ibis::bitvector& res, uint32_t b) const;

    slice(const slice&);
    slice& operator=(const slice&);
}; // ibis::slice

/// The multicomponent range-encoded index.  Defined by Chan and Ioannidis
/// (SIGMOD 98).
class ibis::fade : public ibis::relic {
public:
    virtual ~fade() {clear();};
    fade(const ibis::column* c = 0, const char* f = 0,
	 const uint32_t nbase = 2);
    fade(const ibis::column* c, ibis::fileManager::storage* st,
	 size_t start = 8);

    virtual int write(const char* dt) const;
    virtual void print(std::ostream& out) const;
    virtual int read(const char* idxfile);
    virtual int read(ibis::fileManager::storage* st);

    virtual long append(const char* dt, const char* df, uint32_t nnew);

    using ibis::relic::estimate;
    using ibis::relic::estimateCost;
    virtual long evaluate(const ibis::qContinuousRange& expr,
			  ibis::bitvector& hits) const;
    virtual long evaluate(const ibis::qDiscreteRange& expr,
			  ibis::bitvector& hits) const;

    virtual uint32_t estimate(const ibis::qContinuousRange& expr) const;
    virtual INDEX_TYPE type() const {return FADE;}
    virtual const char* name() const {return "multi-level range";}

    virtual void speedTest(std::ostream& out) const;
    // number of records in each bin
    virtual void binWeights(std::vector<uint32_t>& b) const;
    virtual double getSum() const;
    virtual double estimateCost(const ibis::qContinuousRange& expr) const;
    //virtual double estimateCost(const ibis::qDiscreteRange& expr) const;

protected:
    // protected member variables
    array_t<uint32_t> cnts; // the counts for each distinct value
    array_t<uint32_t> bases;// the values of the bases used

    // protected member functions to be used by derived classes
    int write32(int fdes) const;
    int write64(int fdes) const;
    virtual void clear();
    virtual size_t getSerialSize() const throw();

private:
    // private member functions
    void setBit(const uint32_t i, const double val);
    void construct1(const char* f = 0, const uint32_t nbase = 2);
    void construct2(const char* f = 0, const uint32_t nbase = 2);

    void evalEQ(ibis::bitvector& res, uint32_t b) const;
    void evalLE(ibis::bitvector& res, uint32_t b) const;
    void evalLL(ibis::bitvector& res, uint32_t b0, uint32_t b1) const;

    fade(const fade&);
    fade& operator=(const fade&);
}; // ibis::fade

/// The multicomponent interval encoded index.  Defined by Chan and
/// Ioannidis (SIGMOD 99).
class ibis::sbiad : public ibis::fade {
public:
    virtual ~sbiad() {clear();};
    sbiad(const ibis::column* c = 0, const char* f = 0,
	  const uint32_t nbase = 2);
    sbiad(const ibis::column* c, ibis::fileManager::storage* st,
	  size_t start = 8);

    virtual int write(const char* dt) const;
    virtual void print(std::ostream& out) const;
    virtual long append(const char* dt, const char* df, uint32_t nnew);

    virtual long evaluate(const ibis::qContinuousRange& expr,
			  ibis::bitvector& hits) const;
    virtual long evaluate(const ibis::qDiscreteRange& expr,
			  ibis::bitvector& hits) const;

    virtual INDEX_TYPE type() const {return SBIAD;}
    virtual const char* name() const {return "multi-level interval";}

    virtual void speedTest(std::ostream& out) const;
    //virtual double estimateCost(const ibis::qContinuousRange& expr) const;
    //virtual double estimateCost(const ibis::qDiscreteRange& expr) const;

private:
    // private member functions
    void setBit(const uint32_t i, const double val);
    void construct1(const char* f = 0, const uint32_t nbase = 2);
    void construct2(const char* f = 0, const uint32_t nbase = 2);

    void evalEQ(ibis::bitvector& res, uint32_t b) const;
    void evalLE(ibis::bitvector& res, uint32_t b) const;
    void evalLL(ibis::bitvector& res, uint32_t b0, uint32_t b1) const;

    sbiad(const sbiad&);
    sbiad& operator=(const sbiad&);
}; // ibis::sbiad

/// The multicomponent equality encoded index.
class ibis::sapid : public ibis::fade {
public:
    virtual ~sapid() {clear();};
    sapid(const ibis::column* c = 0, const char* f = 0,
	  const uint32_t nbase = 2);
    sapid(const ibis::column* c, ibis::fileManager::storage* st,
	  size_t start = 8);

    virtual int write(const char* dt) const;
    virtual void print(std::ostream& out) const;
    virtual long append(const char* dt, const char* df, uint32_t nnew);

    virtual long evaluate(const ibis::qContinuousRange& expr,
			  ibis::bitvector& hits) const;
    virtual long evaluate(const ibis::qDiscreteRange& expr,
			  ibis::bitvector& hits) const;

    virtual INDEX_TYPE type() const {return SAPID;}
    virtual const char* name() const {return "multi-level equality";}

    virtual void speedTest(std::ostream& out) const;
    //virtual double estimateCost(const ibis::qContinuousRange& expr) const;
    //virtual double estimateCost(const ibis::qDiscreteRange& expr) const;

private:
    // private member functions
    void setBit(const uint32_t i, const double val);
    void construct1(const char* f = 0, const uint32_t nbase = 2);
    void construct2(const char* f = 0, const uint32_t nbase = 2);

    void addBits_(uint32_t ib, uint32_t ie, ibis::bitvector& res) const;
    void evalEQ(ibis::bitvector& res, uint32_t b) const;
    void evalLE(ibis::bitvector& res, uint32_t b) const;
    void evalLL(ibis::bitvector& res, uint32_t b0, uint32_t b1) const;

    sapid(const sapid&);
    sapid& operator=(const sapid&);
}; // ibis::sapid

/// The two-level interval-equality code.
///
/// @note In fuzzy classification / clustering, many interval equality
/// conditions are used.  Hence the crazy name.
class ibis::fuzz : public ibis::relic {
public:
    virtual ~fuzz() {clear();};
    fuzz(const ibis::column* c = 0, const char* f = 0);
    fuzz(const ibis::column* c, ibis::fileManager::storage* st,
	 size_t start = 8);

    virtual int write(const char* dt) const;
    virtual void print(std::ostream& out) const;
    virtual int read(const char* idxfile);
    virtual int read(ibis::fileManager::storage* st);

    virtual long append(const char* dt, const char* df, uint32_t nnew);

    using ibis::relic::evaluate;
    using ibis::relic::estimate;
    using ibis::relic::estimateCost;
    virtual long evaluate(const ibis::qContinuousRange& expr,
			  ibis::bitvector& hits) const;
    virtual uint32_t estimate(const ibis::qContinuousRange& expr) const;
    virtual double estimateCost(const ibis::qContinuousRange& expr) const;

    virtual INDEX_TYPE type() const {return FUZZ;}
    virtual const char* name() const {return "interval-equality";}

protected:
    virtual void clear();
    virtual size_t getSerialSize() const throw();

private:
    /// The fine level is stored in ibis::relic, the parent class, only
    /// the coarse bins are stored here.  The coarse bins use integer bin
    /// boundaries; these integers are indices to the array vals and bits.
    mutable std::vector<ibis::bitvector*> cbits;
    array_t<uint32_t> cbounds;
    mutable array_t<int32_t> coffset32;
    mutable array_t<int64_t> coffset64;

    void coarsen(); // given fine level, add coarse level
    void activateCoarse() const; // activate all coarse level bitmaps
    void activateCoarse(uint32_t i) const; // activate one bitmap
    void activateCoarse(uint32_t i, uint32_t j) const;

    int writeCoarse32(int fdes) const;
    int writeCoarse64(int fdes) const;
    int readCoarse(const char *fn);
    void clearCoarse();

    /// Estimate the cost of answer a range query [lo, hi).
    long coarseEstimate(uint32_t lo, uint32_t hi) const;
    /// Evaluate the range condition [lo, hi) and place the result in @c res.
    long coarseEvaluate(uint32_t lo, uint32_t hi, ibis::bitvector& res) const;

    fuzz(const fuzz&);
    fuzz& operator=(const fuzz&);
}; // ibis::fuzz

/// The two-level range-equality code.
///
/// @note Bylt is Danish word for pack, the name of the binned version of
/// the two-level range-equality code.
class ibis::bylt : public ibis::relic {
public:
    virtual ~bylt() {clear();};
    bylt(const ibis::column* c = 0, const char* f = 0);
    bylt(const ibis::column* c, ibis::fileManager::storage* st,
	 size_t start = 8);

    virtual int write(const char* dt) const;
    virtual void print(std::ostream& out) const;
    virtual int read(const char* idxfile);
    virtual int read(ibis::fileManager::storage* st);

    virtual long append(const char* dt, const char* df, uint32_t nnew);

    using ibis::relic::evaluate;
    using ibis::relic::estimate;
    using ibis::relic::estimateCost;
    virtual long evaluate(const ibis::qContinuousRange& expr,
			  ibis::bitvector& hits) const;
    virtual uint32_t estimate(const ibis::qContinuousRange& expr) const;
    virtual double estimateCost(const ibis::qContinuousRange& expr) const;

    virtual INDEX_TYPE type() const {return BYLT;}
    virtual const char* name() const {return "range-equality";}

protected:
    virtual void clear();
    virtual size_t getSerialSize() const throw();

private:
    // the fine level is stored in ibis::relic, the parent class, only the
    // coarse bins are stored here.  The coarse bins use integer bin
    // boundaries; these integers are indices to the array vals and bits.
    mutable std::vector<ibis::bitvector*> cbits;
    array_t<uint32_t> cbounds;
    mutable array_t<int32_t> coffset32;
    mutable array_t<int64_t> coffset64;

    void coarsen(); // given fine level, add coarse level
    void activateCoarse() const; // activate all coarse level bitmaps
    void activateCoarse(uint32_t i) const; // activate one bitmap
    void activateCoarse(uint32_t i, uint32_t j) const;

    int writeCoarse32(int fdes) const;
    int writeCoarse64(int fdes) const;
    int readCoarse(const char *fn);

    bylt(const bylt&);
    bylt& operator=(const bylt&);
}; // ibis::bylt

/// The two-level equality-equality code.
/// @note zone is Italian word for zone, the name of the binned version of
/// the two-level equality-equality code.
class ibis::zona : public ibis::relic {
public:
    virtual ~zona() {clear();};
    zona(const ibis::column* c = 0, const char* f = 0);
    zona(const ibis::column* c, ibis::fileManager::storage* st,
	 size_t start = 8);

    virtual int write(const char* dt) const;
    virtual void print(std::ostream& out) const;
    virtual int read(const char* idxfile);
    virtual int read(ibis::fileManager::storage* st);

    virtual long append(const char* dt, const char* df, uint32_t nnew);

    using ibis::relic::evaluate;
    using ibis::relic::estimate;
    using ibis::relic::estimateCost;
    virtual long evaluate(const ibis::qContinuousRange& expr,
			  ibis::bitvector& hits) const;
    virtual uint32_t estimate(const ibis::qContinuousRange& expr) const;
    virtual double estimateCost(const ibis::qContinuousRange& expr) const;

    virtual INDEX_TYPE type() const {return ZONA;}
    virtual const char* name() const {return "equality-equality";}

protected:
    virtual void clear();
    virtual size_t getSerialSize() const throw();

private:
    // the fine level is stored in ibis::relic, the parent class, only the
    // coarse bins are stored here.  The coarse bins use integer bin
    // boundaries; these integers are indices to the array vals and bits.
    mutable std::vector<ibis::bitvector*> cbits;
    array_t<uint32_t> cbounds;
    mutable array_t<int32_t> coffset32;
    mutable array_t<int64_t> coffset64;

    void coarsen(); // given fine level, add coarse level
    void activateCoarse() const; // activate all coarse level bitmaps
    void activateCoarse(uint32_t i) const; // activate one bitmap
    void activateCoarse(uint32_t i, uint32_t j) const;

    int writeCoarse32(int fdes) const;
    int writeCoarse64(int fdes) const;
    int readCoarse(const char *fn);

    zona(const zona&);
    zona& operator=(const zona&);
}; // ibis::zona
#endif // IBIS_IRELIC_H
