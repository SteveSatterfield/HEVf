// File: $Id$
// Author: John Wu <John.Wu at acm.org>
//      Lawrence Berkeley National Laboratory
// Copyright 2006-2009 the Regents of the University of California
//
#include "capi.h"
#include "part.h"	// ibis::part, ibis::column, ibis::tablex
#include "query.h"	// ibis::query
#include "bundle.h"	// ibis::query::result
#include "tafel.h"	// a concrete instance of ibis::tablex

extern "C" {
    struct FastBitQuery {
	const ibis::part *t; //< The ibis::part this query refers to.
	ibis::query q; //< The ibis::query object
	typedef std::map< int, void* > typeValues;
	typedef std::map< const char*, typeValues*, ibis::lessi > valList;
	/// List of values that has been selected and sent to user.
	valList vlist;
    };

    /// A @c FastBitResultSet holds the results of a query in memory and
    /// provides a row-oriented access mechanism for the results.
    ///@note An important limitation is that the current implementation
    /// requires all selected values to be in memory.
    struct FastBitResultSet {
	/// The ibis::query::result object to hold the results in memory.
	ibis::query::result *results;
	/// A place-holder for all the string objects.
	std::vector<std::string> strbuf;
    };
}

class fastbit_part_list {
public:
    fastbit_part_list();
    ~fastbit_part_list();
    ibis::part* find(const char* dir);

private:
    ibis::partAssoc parts;
    pthread_mutex_t mutex;

    void clear();
}; // class fastbit_part_list

// Can not rely on the automatic deallocation of static variable because it
// require the resources hold by another static variable
// (ibis::fileManager::instance()).  The two static variables may be
// deallocated in unpredictable order.  Change to use a local pointer and
// use init and cleanup function to manage its content.
static fastbit_part_list *_capi_tlist=0;
// The pointer to the in-memory buffer used to store new data records.
static ibis::tablex *_capi_tablex=0;

fastbit_part_list::fastbit_part_list() {
    if (0 != pthread_mutex_init
	(&mutex, static_cast<const pthread_mutexattr_t*>(0))) {
	throw "fastbit_part_list unable to initialize the mutex lock";
    }
} // ctor

void fastbit_part_list::clear() {
    ibis::util::mutexLock lock(&mutex, "~fastbit_part_list");
    for (ibis::partAssoc::iterator it = parts.begin();
	 it != parts.end(); ++ it) {
	char* name = const_cast<char*>((*it).first);
	ibis::part* tbl = (*it).second;
	delete [] name;
	delete tbl;
    }
    parts.clear();
} // fastbit_part_list::clear

fastbit_part_list::~fastbit_part_list() {
    clear();
    pthread_mutex_destroy(&mutex);
} // dtor

ibis::part* fastbit_part_list::find(const char* dir) {
    ibis::util::mutexLock lock(&mutex, "fastbit_part_list");
    ibis::partAssoc::const_iterator it = parts.find(dir);
    if (it != parts.end()) {
	return (*it).second;
    }
    else { // need to generate a new table object
	ibis::part *tmp;
	try {
	    tmp = new ibis::part(dir, static_cast<const char*>(0));
	}
	catch (...) {
	    LOGGER(ibis::gVerbose >= 0)
		<< "Warning -- failed to construct a table from directory \""
		<< dir << "\"";
	    tmp = 0;
	}
	if (tmp != 0 && (tmp->name() == 0 || tmp->nRows() == 0 ||
			 tmp->nColumns() == 0)) {
	    LOGGER(ibis::gVerbose > 1)
		<< "Warning -- directory " << dir
		<< " contains an empty data partition";
	    delete tmp;
	    tmp = 0;
	}
	if (tmp != 0) {
	    parts[ibis::util::strnewdup(dir)] = tmp;
	}
	return tmp;
    }
} // fastbit_part_list::find

extern "C" int fastbit_build_indexes(const char *dir, const char *opt) {
    int ierr = 0;
    ibis::part *t;
    if (_capi_tlist == 0) {
	fastbit_init(0);
	if (_capi_tlist == 0) {
	    ierr = -1;
	    return ierr;
	}
    }

    t = _capi_tlist->find(dir);
    if (t->nRows() > 0 && t->nColumns() > 0) {
	t->buildIndexes(opt);
    }
    else {
	LOGGER(ibis::gVerbose > 0)
	    << "fastbit_build_indexes -- data directory \"" << dir
	    << "\" contains no data";
	ierr = 1;
    }
    return ierr;
} // fastbit_build_indexes

extern "C" int fastbit_purge_indexes(const char *dir) {
    int ierr = 0;
    ibis::part *t;
    if (_capi_tlist == 0) {
	fastbit_init(0);
	if (_capi_tlist == 0) {
	    ierr = -1;
	    return ierr;
	}
    }

    t = _capi_tlist->find(dir);
    t->purgeIndexFiles();
    return ierr;
} // fastbit_purge_indexes

extern "C" int fastbit_build_index(const char *dir, const char *att,
				   const char *opt) {
    int ierr = 0;
    ibis::part *t;
    if (_capi_tlist == 0) {
	fastbit_init(0);
	if (_capi_tlist == 0) {
	    ierr = -1;
	    return ierr;
	}
    }

    t = _capi_tlist->find(dir);
    if (t->nRows() == 0 || t->nColumns() == 0) {
	LOGGER(ibis::gVerbose > 0)
	    << "fastbit_build_index -- data directory \"" << dir
	    << "\" contains no data";
	ierr = 1;
	return ierr;
    }

    ibis::column *c = t->getColumn(att);
    if (c == 0) {
	LOGGER(ibis::gVerbose > 0)
	    << "fastbit_build_index -- can not find column \"" << att
	    << "\"in data directory \"" << dir << "\"";
	ierr = -2;
	return ierr;
    }

    if (opt != 0 && *opt != 0)
	c->indexSpec(opt);
    c->loadIndex();
    return ierr;
} // fastbit_build_index

extern "C" int
fastbit_purge_index(const char *dir, const char *att) {
    int ierr = 0;
    ibis::part *t;
    if (_capi_tlist == 0) {
	fastbit_init(0);
	if (_capi_tlist == 0) {
	    ierr = -1;
	    return ierr;
	}
    }

    t = _capi_tlist->find(dir);
    if (t->nRows() == 0 || t->nColumns() == 0) {
	LOGGER(ibis::gVerbose > 0)
	    << "fastbit_purge_index -- data directory \"" << dir
	    << "\" contains no data";
	ierr = 1;
	return ierr;
    }

    ibis::column *c = t->getColumn(att);
    if (c == 0) {
	ierr = -2;
	return ierr;
    }

    c->purgeIndexFile();
    return ierr;
} // fastbit_purge_index

extern "C" FastBitQueryHandle
fastbit_build_query(const char *select, const char *from, const char *where) {
    if (from == 0 || where == 0 || *from == 0 || *where == 0)
	return 0;
    if (_capi_tlist == 0) {
	fastbit_init(0);
	if (_capi_tlist == 0) {
	    return 0;
	}
    }

    FastBitQueryHandle h = new FastBitQuery;
    h->t = _capi_tlist->find(from);
    if (h->t == 0) {
	LOGGER(ibis::gVerbose >= 0)
	    << "Warning -- fastbit_build_query failed to generate table "
	    "object from data directory \"" << from << "\"";
	delete h;
	h = 0;
	return h;
    }

    int ierr = h->q.setPartition(h->t);
    if (ierr < 0) {
	LOGGER(ibis::gVerbose >= 0)
	    << "Warning -- fastbit_build_query failed to assign an "
	    << "ibis::part (" << h->t->name() << ") object to a query";
	fastbit_destroy_query(h);
	h = 0;
	return h;
    }

    ierr = h->q.setWhereClause(where);
    if (ierr < 0) {
	LOGGER(ibis::gVerbose >= 0)
	    << "Warning -- fastbit_build_query failed to assign "
	    << "conditions (" << where << ") to a query";
	fastbit_destroy_query(h);
	h = 0;
	return h;
    }

    if (select != 0 && *select != 0) {
	ierr = h->q.setSelectClause(select);
	if (ierr < 0) {
	    LOGGER(ibis::gVerbose > 0)
		<< "fastbit_build_query -- failed to assign a select "
		<< "clause (" << select << ") to a query";
	}
    }

    // evaluate the query here
    ierr = h->q.evaluate();
    if (ierr < 0) {
	fastbit_destroy_query(h);
	h = 0;
	return h;
    }
    return h;
} // fastbit_build_query

extern "C" int
fastbit_destroy_query(FastBitQueryHandle qhandle) {
    // first delete the list of values
    for (FastBitQuery::valList::iterator it = qhandle->vlist.begin();
	 it != qhandle->vlist.end(); ++ it) {
	FastBitQuery::typeValues& tv = *((*it).second);
	for (FastBitQuery::typeValues::iterator tvit = tv.begin();
	     tvit != tv.end(); ++ tvit) {
	    switch ((ibis::TYPE_T) (*tvit).first) {
	    case ibis::DOUBLE: {
		ibis::array_t<double> *tmp =
		    static_cast<ibis::array_t<double>*>((*tvit).second);
		delete tmp;
		break;}
	    case ibis::FLOAT: {
		ibis::array_t<float> *tmp = 
		    static_cast<ibis::array_t<float>*>((*tvit).second);
		delete tmp;
		break;}
	    case ibis::OID: {
		ibis::array_t<ibis::rid_t> *tmp =
		    static_cast<ibis::array_t<ibis::rid_t>*>((*tvit).second);
		delete tmp;
		break;}
	    case ibis::BYTE: {
		ibis::array_t<char> *btmp =
		    static_cast<ibis::array_t<char>*>((*tvit).second);
		delete btmp;
		break;}
	    case ibis::SHORT: {
		ibis::array_t<int16_t> *stmp =
		    static_cast<ibis::array_t<int16_t>*>((*tvit).second);
		delete stmp;
		break;}
	    case ibis::INT: {
		ibis::array_t<int32_t> *tmp =
		    static_cast<ibis::array_t<int32_t>*>((*tvit).second);
		delete tmp;
		break;}
	    case ibis::LONG: {
		ibis::array_t<int64_t> *ltmp =
		    static_cast<ibis::array_t<int64_t>*>((*tvit).second);
		delete ltmp;
		break;}
	    case ibis::UBYTE:  {
		ibis::array_t<unsigned char> *btmp =
		    static_cast<ibis::array_t<unsigned char>*>((*tvit).second);
		delete btmp;
		break;}
	    case ibis::USHORT: {
		ibis::array_t<uint16_t> *stmp =
		    static_cast<ibis::array_t<uint16_t>*>((*tvit).second);
		delete stmp;
		break;}
	    case ibis::UINT: {
		ibis::array_t<uint32_t> *tmp =
		    static_cast<ibis::array_t<uint32_t>*>((*tvit).second);
		delete tmp;
		break;}
	    case ibis::ULONG: {
		ibis::array_t<uint64_t> *ltmp =
		    static_cast<ibis::array_t<uint64_t>*>((*tvit).second);
		delete ltmp;
		break;}
	    default: {
		LOGGER(ibis::gVerbose >= 0)
		    << "Warning -- column type " << (*tvit).first
		    << " not supported";
		break;}
	    }
	} // tvit
	delete (*it).second;
    } // it

    // finally remove the FastBitQuery object itself
    delete qhandle;
    return 0;
} // fastbit_destroy_query

extern "C" int fastbit_get_result_rows(FastBitQueryHandle qhandle) {
    int ret = -1;
    if (qhandle == 0) return ret;
    if (qhandle->t == 0)
	return ret;
    if (qhandle->q.getState() != ibis::query::FULL_EVALUATE)
	qhandle->q.evaluate();

    ret = qhandle->q.getNumHits();
    return ret;
} // fastbit_get_result_rows

extern "C" int
fastbit_get_result_columns(FastBitQueryHandle qhandle) {
    int ret = -1;
    if (qhandle == 0)
	return ret;
    ret = qhandle->q.components().size();
    return ret;
} // fastbit_get_result_columns

extern "C" const char*
fastbit_get_select_clause(FastBitQueryHandle qhandle) {
    const char* tmp = 0;
    if (qhandle == 0)
	return tmp;
    tmp = qhandle->q.getSelectClause();
    return tmp;
} // fastbit_get_select_clause

extern "C" const char*
fastbit_get_from_clause(FastBitQueryHandle qhandle) {
    const char* tmp = 0;
    if (qhandle == 0)
	return tmp;
    tmp = qhandle->q.partition()->name();
    return tmp;
} // fastbit_get_from_clause

extern "C" const char*
fastbit_get_where_clause(FastBitQueryHandle qhandle) {
    const char* tmp = 0;
    if (qhandle == 0)
	return tmp;
    tmp = qhandle->q.getWhereClause();
    return tmp;
} // fastbit_get_where_clause

extern "C" const char *
fastbit_get_qualified_bytes(FastBitQueryHandle qhandle, const char *att) {
    const char *ret = 0;
    if (qhandle == 0 || att == 0 || *att == 0) return ret;
    if (qhandle->t == 0 ||
	qhandle->q.getState() != ibis::query::FULL_EVALUATE) {
	LOGGER(ibis::gVerbose > 0)
	    << "fastbit_get_qualified_bytes -- invalid query handle ("
	    << qhandle << ")";
	return ret;
    }

    const ibis::column *c = qhandle->t->getColumn(att);
    if (c == 0) {
	LOGGER(ibis::gVerbose > 0)
	    << "fastbit_get_qualified_bytes -- can not find a column "
	    << "named \"" << att << "\"";
	return ret;
    }
    if (c->type() != ibis::BYTE) {
	LOGGER(ibis::gVerbose > 0)
	    << "fastbit_get_qualified_bytes -- column \"" << att
	    << "\" has type " << ibis::TYPESTRING[(int)c->type()]
	    << ", expect type BYTE";
	return ret;
    }

    FastBitQuery::valList::const_iterator it = qhandle->vlist.find(att);
    if (it != qhandle->vlist.end()) {
	const FastBitQuery::typeValues *tv = (*it).second;
	FastBitQuery::typeValues::const_iterator tvit =
	    tv->find((int) ibis::BYTE);
	if (tvit == tv->end())
	    tvit = tv->find((int) ibis::UBYTE);
	if (tvit != tv->end()) {
	    LOGGER(ibis::gVerbose > 3)
		<< "fastbit_get_qualified_bytes -- found column \""
		<< att << "\" in the existing list";
	    ret = static_cast<ibis::array_t<char>*>((*tvit).second)->begin();
	}
    }
    if (ret == 0) {
	ibis::array_t<char> *tmp = c->selectBytes(*(qhandle->q.getHitVector()));
	if (tmp == 0 || tmp->empty()) {
	    delete tmp;
	}
	else {
	    ibis::query::writeLock
		lock(&(qhandle->q), "fastbit_get_qualified_bytes");
	    it = qhandle->vlist.find(att);
	    if (it == qhandle->vlist.end()) {
		FastBitQuery::typeValues *tv = new FastBitQuery::typeValues;
		(*tv)[(int) ibis::BYTE] = static_cast<void*>(tmp);
		qhandle->vlist[c->name()] = tv;
		ret = tmp->begin();
	    }
	    else {
		FastBitQuery::typeValues *tv = (*it).second;
		(*tv)[(int) ibis::BYTE] = static_cast<void*>(tmp);
		ret = tmp->begin();
	    }
	}
    }
    return ret;
} // fastbit_get_qualified_bytes

extern "C" const int16_t *
fastbit_get_qualified_shorts(FastBitQueryHandle qhandle, const char *att) {
    const int16_t *ret = 0;
    if (qhandle == 0 || att == 0 || *att == 0) return ret;
    if (qhandle->t == 0 ||
	qhandle->q.getState() != ibis::query::FULL_EVALUATE) {
	LOGGER(ibis::gVerbose > 0)
	    << "fastbit_get_qualified_shorts -- invalid query handle ("
	    << qhandle << ")";
	return ret;
    }

    const ibis::column *c = qhandle->t->getColumn(att);
    if (c == 0) {
	LOGGER(ibis::gVerbose > 0)
	    << "fastbit_get_qualified_shorts -- can not find a column "
	    << "named \"" << att << "\"";
	return ret;
    }
    if (c->type() != ibis::BYTE &&
	c->type() != ibis::UBYTE &&
	c->type() != ibis::SHORT) {
	LOGGER(ibis::gVerbose > 0)
	    << "fastbit_get_qualified_shorts -- column \"" << att
	    << "\" has type " << ibis::TYPESTRING[(int)c->type()]
	    << ", expect type SHORT or BYTE";
	return ret;
    }

    FastBitQuery::valList::const_iterator it = qhandle->vlist.find(att);
    if (it != qhandle->vlist.end()) {
	const FastBitQuery::typeValues *tv = (*it).second;
	FastBitQuery::typeValues::const_iterator tvit =
	    tv->find((int) ibis::SHORT);
	if (tvit == tv->end())
	    tvit = tv->find((int) ibis::USHORT);
	if (tvit != tv->end()) {
	    LOGGER(ibis::gVerbose > 3)
		<< "fastbit_get_qualified_shorts -- found column \""
		<< att << "\" in the existing list";
	    ret = static_cast<ibis::array_t<int16_t>*>((*tvit).second)->begin();
	}
    }
    if (ret == 0) {
	ibis::array_t<int16_t> *tmp =
	    c->selectShorts(*(qhandle->q.getHitVector()));
	if (tmp == 0 || tmp->empty()) {
	    delete tmp;
	}
	else {
	    ibis::query::writeLock
		lock(&(qhandle->q), "fastbit_get_qualified_shorts");
	    it = qhandle->vlist.find(att);
	    if (it == qhandle->vlist.end()) {
		FastBitQuery::typeValues *tv = new FastBitQuery::typeValues;
		(*tv)[(int) ibis::SHORT] = static_cast<void*>(tmp);
		qhandle->vlist[c->name()] = tv;
	    }
	    else {
		FastBitQuery::typeValues *tv = (*it).second;
		(*tv)[(int) ibis::SHORT] = static_cast<void*>(tmp);
	    }
	    ret = tmp->begin();
	}
    }
    return ret;
} // fastbit_get_qualified_shorts

extern "C" const int32_t *
fastbit_get_qualified_ints(FastBitQueryHandle qhandle, const char *att) {
    const int32_t *ret = 0;
    if (qhandle == 0 || att == 0 || *att == 0) return ret;
    if (qhandle->t == 0 ||
	qhandle->q.getState() != ibis::query::FULL_EVALUATE) {
	LOGGER(ibis::gVerbose > 0)
	    << "fastbit_get_qualified_ints -- invalid query handle ("
	    << qhandle << ")";
	return ret;
    }

    const ibis::column *c = qhandle->t->getColumn(att);
    if (c == 0) {
	LOGGER(ibis::gVerbose > 0)
	    << "fastbit_get_qualified_ints -- can not find a column "
	    << "named \"" << att << "\"";
	return ret;
    }
    if (c->type() != ibis::INT &&
	c->type() != ibis::BYTE &&
	c->type() != ibis::UBYTE &&
	c->type() != ibis::SHORT &&
	c->type() != ibis::USHORT) {
	LOGGER(ibis::gVerbose > 0)
	    << "fastbit_get_qualified_ints -- column \"" << att
	    << "\" has type " << ibis::TYPESTRING[(int)c->type()]
	    << ", expect type INT or shorter integer types";
	return ret;
    }

    FastBitQuery::valList::const_iterator it = qhandle->vlist.find(att);
    if (it != qhandle->vlist.end()) {
	const FastBitQuery::typeValues *tv = (*it).second;
	FastBitQuery::typeValues::const_iterator tvit =
	    tv->find((int) ibis::INT);
	if (tvit == tv->end())
	    tvit = tv->find((int) ibis::UINT);
	if (tvit != tv->end()) {
	    LOGGER(ibis::gVerbose > 3)
		<< "fastbit_get_qualified_ints -- found column \"" << att
		<< "\" in the existing list";
	    ret = static_cast<ibis::array_t<int32_t>*>((*tvit).second)->begin();
	}
    }
    if (ret == 0) {
	ibis::array_t<int32_t> *tmp =
	    c->selectInts(*(qhandle->q.getHitVector()));
	if (tmp == 0 || tmp->empty()) {
	    delete tmp;
	}
	else {
	    LOGGER(ibis::gVerbose > 3)
		<< "fastbit_get_qualified_ints -- retrieved "
		<< tmp->size() << " value" << (tmp->size()>1 ? "s" : "")
		<< " of " << att << " from "
		<< c->partition()->currentDataDir();
	    ibis::query::writeLock
		lock(&(qhandle->q), "fastbit_get_qualified_ints");
	    it = qhandle->vlist.find(att);
	    if (it == qhandle->vlist.end()) {
		FastBitQuery::typeValues *tv = new FastBitQuery::typeValues;
		(*tv)[(int) ibis::INT] = static_cast<void*>(tmp);
		qhandle->vlist[c->name()] = tv;
	    }
	    else {
		FastBitQuery::typeValues *tv = (*it).second;
		(*tv)[(int) ibis::INT] = static_cast<void*>(tmp);
	    }
	    ret = tmp->begin();
	}
    }
    return ret;
} // fastbit_get_qualified_ints

extern "C" const int64_t *
fastbit_get_qualified_longs(FastBitQueryHandle qhandle, const char *att) {
    const int64_t *ret = 0;
    if (qhandle == 0 || att == 0 || *att == 0) return ret;
    if (qhandle->t == 0 ||
	qhandle->q.getState() != ibis::query::FULL_EVALUATE) {
	LOGGER(ibis::gVerbose > 0)
	    << "fastbit_get_qualified_longs -- invalid query handle ("
	    << qhandle << ")";
	return ret;
    }

    const ibis::column *c = qhandle->t->getColumn(att);
    if (c == 0) {
	LOGGER(ibis::gVerbose > 0)
	    << "fastbit_get_qualified_longs -- can not find a column "
	    << "named \"" << att << "\"";
	return ret;
    }
    if (c->type() != ibis::LONG &&
	c->type() != ibis::INT &&
	c->type() != ibis::UINT &&
	c->type() != ibis::BYTE &&
	c->type() != ibis::UBYTE &&
	c->type() != ibis::SHORT &&
	c->type() != ibis::USHORT &&
	c->type() != ibis::TEXT &&
	c->type() != ibis::CATEGORY) {
	LOGGER(ibis::gVerbose > 0)
	    << "fastbit_get_qualified_longs -- column \"" << att
	    << "\" has type " << ibis::TYPESTRING[(int)c->type()]
	    << ", expect type LONG or a compatible type";
	return ret;
    }

    FastBitQuery::valList::const_iterator it = qhandle->vlist.find(att);
    if (it != qhandle->vlist.end()) {
	const FastBitQuery::typeValues *tv = (*it).second;
	FastBitQuery::typeValues::const_iterator tvit =
	    tv->find((int) ibis::LONG);
	if (tvit == tv->end())
	    tvit = tv->find((int) ibis::ULONG);
	if (tvit != tv->end()) {
	    LOGGER(ibis::gVerbose > 3)
		<< "fastbit_get_qualified_longs -- found column \""
		<< att << "\" in the existing list";
	    ret = static_cast<ibis::array_t<int64_t>*>((*tvit).second)->begin();
	}
    }
    if (ret == 0) {
	ibis::array_t<int64_t> *tmp =
	    c->selectLongs(*(qhandle->q.getHitVector()));
	if (tmp == 0 || tmp->empty()) {
	    delete tmp;
	}
	else {
	    ibis::query::writeLock
		lock(&(qhandle->q), "fastbit_get_qualified_longs");
	    it = qhandle->vlist.find(att);
	    if (it == qhandle->vlist.end()) {
		FastBitQuery::typeValues *tv = new FastBitQuery::typeValues;
		(*tv)[(int) ibis::LONG] = static_cast<void*>(tmp);
		qhandle->vlist[c->name()] = tv;
	    }
	    else {
		FastBitQuery::typeValues *tv = (*it).second;
		(*tv)[(int) ibis::LONG] = static_cast<void*>(tmp);
	    }
	    ret = tmp->begin();
	}
    }
    return ret;
} // fastbit_get_qualified_longs

extern "C" const unsigned char *
fastbit_get_qualified_ubytes(FastBitQueryHandle qhandle, const char *att) {
    const unsigned char *ret = 0;
    if (qhandle == 0 || att == 0 || *att == 0) return ret;
    if (qhandle->t == 0 ||
	qhandle->q.getState() != ibis::query::FULL_EVALUATE) {
	LOGGER(ibis::gVerbose > 0)
	    << "fastbit_get_qualified_ubytes -- invalid query handle ("
	    << qhandle << ")";
	return ret;
    }

    const ibis::column *c = qhandle->t->getColumn(att);
    if (c == 0) {
	LOGGER(ibis::gVerbose > 0)
	    << "fastbit_get_qualified_ubytes -- can not find a column "
	    << "named \"" << att << "\"";
	return ret;
    }
    if (c->type() != ibis::UBYTE) {
	LOGGER(ibis::gVerbose > 0)
	    << "fastbit_get_qualified_ubytes -- column \"" << att
	    << "\" has type " << ibis::TYPESTRING[(int)c->type()]
	    << ", expect type UBYTE";
	return ret;
    }

    FastBitQuery::valList::const_iterator it = qhandle->vlist.find(att);
    if (it != qhandle->vlist.end()) {
	const FastBitQuery::typeValues *tv = (*it).second;
	FastBitQuery::typeValues::const_iterator tvit =
	    tv->find((int) ibis::UBYTE);
	if (tvit == tv->end())
	    tvit = tv->find((int) ibis::BYTE);
	if (tvit != tv->end()) {
	    LOGGER(ibis::gVerbose > 3)
		<< "fastbit_get_qualified_ubytes -- found column \""
		<< att << "\" in the existing list";
	    ret = static_cast<ibis::array_t<unsigned char>*>
		((*tvit).second)->begin();
	}
    }
    if (ret == 0) {
	ibis::array_t<unsigned char> *tmp =
	    c->selectUBytes(*(qhandle->q.getHitVector()));
	if (tmp == 0 || tmp->empty()) {
	    delete tmp;
	}
	else {
	    ibis::query::writeLock
		lock(&(qhandle->q), "fastbit_get_qualified_ubytes");
	    it = qhandle->vlist.find(att);
	    if (it == qhandle->vlist.end()) {
		FastBitQuery::typeValues *tv = new FastBitQuery::typeValues;
		(*tv)[(int) ibis::UBYTE] = static_cast<void*>(tmp);
		qhandle->vlist[c->name()] = tv;
	    }
	    else {
		FastBitQuery::typeValues *tv = (*it).second;
		(*tv)[(int) ibis::UBYTE] = static_cast<void*>(tmp);
	    }
	    ret = tmp->begin();
	}
    }
    return ret;
} // fastbit_get_qualified_ubytes

extern "C" const uint16_t *
fastbit_get_qualified_ushorts(FastBitQueryHandle qhandle, const char *att) {
    const uint16_t *ret = 0;
    if (qhandle == 0 || att == 0 || *att == 0) return ret;
    if (qhandle->t == 0 ||
	qhandle->q.getState() != ibis::query::FULL_EVALUATE) {
	LOGGER(ibis::gVerbose > 0)
	    << "fastbit_get_qualified_ushorts -- invalid query handle ("
	    << qhandle << ")";
	return ret;
    }

    const ibis::column *c = qhandle->t->getColumn(att);
    if (c == 0) {
	LOGGER(ibis::gVerbose > 0)
	    << "fastbit_get_qualified_ushorts -- can not find a column "
	    << "named \"" << att << "\"";
	return ret;
    }
    if (c->type() != ibis::USHORT &&
	c->type() != ibis::BYTE &&
	c->type() != ibis::UBYTE) {
	LOGGER(ibis::gVerbose > 0)
	    << "fastbit_get_qualified_ushorts -- column \"" << att
	    << "\" has type " << ibis::TYPESTRING[(int)c->type()]
	    << ", expect type USHORT or BYTE";
	return ret;
    }

    FastBitQuery::valList::const_iterator it = qhandle->vlist.find(att);
    if (it != qhandle->vlist.end()) {
	const FastBitQuery::typeValues *tv = (*it).second;
	FastBitQuery::typeValues::const_iterator tvit =
	    tv->find((int) ibis::USHORT);
	if (tvit == tv->end())
	    tvit = tv->find((int) ibis::SHORT);
	if (tvit != tv->end()) {
	    LOGGER(ibis::gVerbose > 3)
		<< "fastbit_get_qualified_ushorts -- found column \""
		<< att << "\" in the existing list";
	    ret = static_cast<ibis::array_t<uint16_t>*>
		((*tvit).second)->begin();
	}
    }
    if (ret == 0) {
	ibis::array_t<uint16_t> *tmp =
	    c->selectUShorts(*(qhandle->q.getHitVector()));
	if (tmp == 0 || tmp->empty()) {
	    delete tmp;
	}
	else {
	    ibis::query::writeLock
		lock(&(qhandle->q), "fastbit_get_qualified_ushorts");
	    it = qhandle->vlist.find(att);
	    if (it == qhandle->vlist.end()) {
		FastBitQuery::typeValues *tv = new FastBitQuery::typeValues;
		(*tv)[(int) ibis::USHORT] = static_cast<void*>(tmp);
		qhandle->vlist[c->name()] = tv;
	    }
	    else {
		FastBitQuery::typeValues *tv = (*it).second;
		(*tv)[(int) ibis::USHORT] = static_cast<void*>(tmp);
	    }
	    ret = tmp->begin();
	}
    }
    return ret;
} // fastbit_get_qualified_ushorts

extern "C" const uint32_t *
fastbit_get_qualified_uints(FastBitQueryHandle qhandle, const char *att) {
    const uint32_t *ret = 0;
    if (qhandle == 0 || att == 0 || *att == 0) return ret;
    if (qhandle->t == 0 ||
	qhandle->q.getState() != ibis::query::FULL_EVALUATE) {
	LOGGER(ibis::gVerbose > 0)
	    << "fastbit_get_qualified_uints -- invalid query handle ("
	    << qhandle << ")";
	return ret;
    }

    const ibis::column *c = qhandle->t->getColumn(att);
    if (c == 0) {
	LOGGER(ibis::gVerbose > 0)
	    << "fastbit_get_qualified_uints -- can not find a column "
	    << "named \"" << att << "\"";
	return ret;
    }
    if (c->type() != ibis::UINT &&
	c->type() != ibis::USHORT &&
	c->type() != ibis::UBYTE &&
	c->type() != ibis::SHORT &&
	c->type() != ibis::BYTE) {
	LOGGER(ibis::gVerbose > 0)
	    << "fastbit_get_qualified_uints -- column \"" << att
	    << "\" has type " << ibis::TYPESTRING[(int)c->type()]
	    << ", expect type UINT or shoter integer types";
	return ret;
    }

    FastBitQuery::valList::const_iterator it = qhandle->vlist.find(att);
    if (it != qhandle->vlist.end()) {
	const FastBitQuery::typeValues *tv = (*it).second;
	FastBitQuery::typeValues::const_iterator tvit =
	    tv->find((int) ibis::UINT);
	if (tvit == tv->end())
	    tvit = tv->find((int) ibis::INT);
	if (tvit != tv->end()) {
	    LOGGER(ibis::gVerbose > 3)
		<< "fastbit_get_qualified_uints -- found column \""
		<< att << "\" in the existing list";
	    ret = static_cast<ibis::array_t<uint32_t>*>
		((*tvit).second)->begin();
	}
    }
    if (ret == 0) {
	ibis::array_t<uint32_t> *tmp =
	    c->selectUInts(*(qhandle->q.getHitVector()));
	if (tmp == 0 || tmp->empty()) {
	    delete tmp;
	}
	else {
	    ibis::query::writeLock
		lock(&(qhandle->q), "fastbit_get_qualified_uints");
	    it = qhandle->vlist.find(att);
	    if (it == qhandle->vlist.end()) {
		FastBitQuery::typeValues *tv = new FastBitQuery::typeValues;
		(*tv)[(int) ibis::UINT] = static_cast<void*>(tmp);
		qhandle->vlist[c->name()] = tv;
	    }
	    else {
		FastBitQuery::typeValues *tv = (*it).second;
		(*tv)[(int) ibis::UINT] = static_cast<void*>(tmp);
	    }
	    ret = tmp->begin();
	}
    }
    return ret;
} // fastbit_get_qualified_uints

extern "C" const uint64_t *
fastbit_get_qualified_ulongs(FastBitQueryHandle qhandle, const char *att) {
    const uint64_t *ret = 0;
    if (qhandle == 0 || att == 0 || *att == 0) return ret;
    if (qhandle->t == 0 ||
	qhandle->q.getState() != ibis::query::FULL_EVALUATE) {
	LOGGER(ibis::gVerbose > 0)
	    << "fastbit_get_qualified_ulongs -- invalid query handle ("
	    << qhandle << ")";
	return ret;
    }

    const ibis::column *c = qhandle->t->getColumn(att);
    if (c == 0) {
	LOGGER(ibis::gVerbose > 0)
	    << "fastbit_get_qualified_ulongs -- can not find a column "
	    << "named \"" << att << "\"";
	return ret;
    }
    if (c->type() != ibis::ULONG &&
	c->type() != ibis::UINT &&
	c->type() != ibis::USHORT &&
	c->type() != ibis::UBYTE &&
	c->type() != ibis::INT &&
	c->type() != ibis::SHORT &&
	c->type() != ibis::BYTE) {
	LOGGER(ibis::gVerbose > 0)
	    << "fastbit_get_qualified_ulongs -- column \"" << att
	    << "\" has type " << ibis::TYPESTRING[(int)c->type()]
	    << ", expect type ULONG or shorter integer types";
	return ret;
    }

    FastBitQuery::valList::const_iterator it = qhandle->vlist.find(att);
    if (it != qhandle->vlist.end()) {
	const FastBitQuery::typeValues *tv = (*it).second;
	FastBitQuery::typeValues::const_iterator tvit =
	    tv->find((int) ibis::ULONG);
	if (tvit == tv->end())
	    tvit = tv->find((int) ibis::LONG);
	if (tvit != tv->end()) {
	    LOGGER(ibis::gVerbose > 3)
		<< "fastbit_get_qualified_ulongs -- found column \""
		<< att << "\" in the existing list";
	    ret = static_cast<ibis::array_t<uint64_t>*>
		((*tvit).second)->begin();
	}
    }
    if (ret == 0) {
	ibis::array_t<uint64_t> *tmp =
	    c->selectULongs(*(qhandle->q.getHitVector()));
	if (tmp == 0 || tmp->empty()) {
	    delete tmp;
	}
	else {
	    ibis::query::writeLock
		lock(&(qhandle->q), "fastbit_get_qualified_ulongs");
	    it = qhandle->vlist.find(att);
	    if (it == qhandle->vlist.end()) {
		FastBitQuery::typeValues *tv = new FastBitQuery::typeValues;
		(*tv)[(int) ibis::ULONG] = static_cast<void*>(tmp);
		qhandle->vlist[c->name()] = tv;
	    }
	    else {
		FastBitQuery::typeValues *tv = (*it).second;
		(*tv)[(int) ibis::ULONG] = static_cast<void*>(tmp);
	    }
	    ret = tmp->begin();
	}
    }
    return ret;
} // fastbit_get_qualified_ulongs

extern "C" const float *
fastbit_get_qualified_floats(FastBitQueryHandle qhandle, const char *att) {
    const float *ret = 0;
    if (qhandle == 0 || att == 0 || *att == 0) return ret;
    if (qhandle->t == 0 ||
	qhandle->q.getState() != ibis::query::FULL_EVALUATE) {
	LOGGER(ibis::gVerbose > 0)
	    << "fastbit_get_qualified_floats -- invalid query handle ("
	    << qhandle << ")";
	return ret;
    }

    const ibis::column *c = qhandle->t->getColumn(att);
    if (c == 0) {
	LOGGER(ibis::gVerbose > 0)
	    << "fastbit_get_qualified_floats -- can not find a column "
	    << "named \"" << att << "\"";
	return ret;
    }
    if (c->type() != ibis::FLOAT) {
	LOGGER(ibis::gVerbose > 0)
	    << "fastbit_get_qualified_floats -- column \"" << att
	    << "\" has type " << ibis::TYPESTRING[(int)c->type()]
	    << ", expect type FLOAT or short integer types";
	return ret;
    }

    FastBitQuery::valList::const_iterator it = qhandle->vlist.find(att);
    if (it != qhandle->vlist.end()) {
	const FastBitQuery::typeValues *tv = (*it).second;
	FastBitQuery::typeValues::const_iterator tvit =
	    tv->find((int) ibis::FLOAT);
	if (tvit != tv->end()) {
	    LOGGER(ibis::gVerbose > 3)
		<< "fastbit_get_qualified_ulongs -- found column \""
		<< att << "\" in the existing list";
	    ret = static_cast<ibis::array_t<float>*>((*tvit).second)->begin();
	}
    }
    if (ret == 0) {// need to read the files
	ibis::array_t<float> *tmp =
	    c->selectFloats(*(qhandle->q.getHitVector()));
	if (tmp == 0 || tmp->empty()) {
	    delete tmp;
	}
	else {
	    ibis::query::writeLock
		lock(&(qhandle->q), "fastbit_get_qualified_floats");
	    it = qhandle->vlist.find(att);
	    if (it == qhandle->vlist.end()) {
		FastBitQuery::typeValues *tv = new FastBitQuery::typeValues;
		(*tv)[(int) ibis::FLOAT] = static_cast<void*>(tmp);
		qhandle->vlist[c->name()] = tv;
	    }
	    else {
		FastBitQuery::typeValues *tv = (*it).second;
		(*tv)[(int) ibis::FLOAT] = static_cast<void*>(tmp);
	    }
	    ret = tmp->begin();
	}
    }
    return ret;
} // fastbit_get_qualified_floats

extern "C" const double *
fastbit_get_qualified_doubles(FastBitQueryHandle qhandle, const char *att) {
    const double *ret = 0;
    if (qhandle == 0 || att == 0 || *att == 0) return ret;
    if (qhandle->t == 0 ||
	qhandle->q.getState() != ibis::query::FULL_EVALUATE) {
	LOGGER(ibis::gVerbose > 0)
	    << "fastbit_get_qualified_doubles -- invalid query handle ("
	    << qhandle << ")";
	return ret;
    }

    const ibis::column *c = qhandle->t->getColumn(att);
    if (c == 0) {
	LOGGER(ibis::gVerbose > 0)
	    << "fastbit_get_qualified_doubles -- can not find a column "
	    << "named \"" << att << "\"";
	return ret;
    }
    if (c->type() == ibis::CATEGORY || c->type() == ibis::TEXT) {
	LOGGER(ibis::gVerbose > 0)
	    << "fastbit_get_qualified_doubles -- column \"" << att
	    << "\" has type " << ibis::TYPESTRING[(int)c->type()]
	    << ", expect type DOUBLE or shorter numerical values";
	return ret;
    }

    FastBitQuery::valList::const_iterator it = qhandle->vlist.find(att);
    if (it != qhandle->vlist.end()) {
	const FastBitQuery::typeValues *tv = (*it).second;
	FastBitQuery::typeValues::const_iterator tvit =
	    tv->find((int) ibis::DOUBLE);
	if (tvit != tv->end()) {
	    LOGGER(ibis::gVerbose > 3)
		<< "fastbit_get_qualified_ulongs -- found column \""
		<< att << "\" in the existing list";
	    ret = static_cast<ibis::array_t<double>*>((*tvit).second)->begin();
	}
    }
    else {
	ibis::array_t<double> *tmp =
	    c->selectDoubles(*(qhandle->q.getHitVector()));
	if (tmp == 0 || tmp->empty()) {
	    delete tmp;
	}
	else {
	    ibis::query::writeLock
		lock(&(qhandle->q), "fastbit_get_qualified_doubles");
	    it = qhandle->vlist.find(att);
	    if (it == qhandle->vlist.end()) {
		FastBitQuery::typeValues *tv = new FastBitQuery::typeValues;
		(*tv)[(int) ibis::DOUBLE] = static_cast<void*>(tmp);
		qhandle->vlist[c->name()] = tv;
	    }
	    else {
		FastBitQuery::typeValues *tv = (*it).second;
		(*tv)[(int) ibis::DOUBLE] = static_cast<void*>(tmp);
	    }
	    ret = tmp->begin();
	}
    }
    return ret;
} // fastbit_get_qualified_doubles

/// This initialization function may optionally read a configuration file.
/// May pass in a nil pointer as rcfile if one is expected to use use the
/// default configuartion files listed in the documentation of
/// ibis::resources::read.  One may call this function multiple times to
/// read multiple configuration files to modify the parameters.
extern "C" void fastbit_init(const char *rcfile) {
    ibis::util::mutexLock lock(&ibis::util::envLock, "fastbit_init");
    if (rcfile && *rcfile)
	ibis::gParameters().read(rcfile);
    if (_capi_tlist == 0)
	_capi_tlist = new fastbit_part_list;
} // fastbit_init

/// This function releases the list of data partitions.  It is expected to
/// be te last function to be called by the user.  Since there is no
/// centralized list of query objects, the user is responsible for freeing
/// the resources held by each query object.
extern "C" void fastbit_cleanup(void) {
    ibis::util::mutexLock lock(&ibis::util::envLock, "fastbit_cleanup");
    if (_capi_tlist != 0) {
	delete _capi_tlist;
	// assign it to 0 so that we can call fastbit_init again
	_capi_tlist = 0;
	ibis::fileManager::instance().clear();
	ibis::util::closeLogFile();
    }
    if (_capi_tablex != 0) {
	LOGGER(ibis::gVerbose > 0)
	    << "fastbit_cleanup is removing a non-empty data buffer "
	    "for new records";
	delete _capi_tablex;
    }
} // fastbit_cleanup

/// Returns the old verboseness level.
extern "C" int fastbit_set_verbose_level(int v) {
    int ret = ibis::gVerbose;
    ibis::gVerbose = v;
    return ret;
} // fastbit_set_verbose_level

extern "C" int fastbit_get_verbose_level(void) {
    return ibis::gVerbose;
} // fastbit_get_verbose_level

/// Return 0 to indicate success, a negative value to indicate error.
extern "C" int fastbit_set_logfile(const char* filename) {
    return ibis::util::setLogFileName(filename);
} // fastbit_set_logfile

/// Return the current log file name.  A blank string or a null pointer
/// indicates the standard output.
extern "C" const char* fastbit_get_logfile() {
    return ibis::util::getLogFileName();
} // fastbit_get_logfile

extern "C" FILE* fastbit_get_logfilepointer() {
    return ibis::util::getLogFile();
} // fastbit_get_logfilepointer

/// Create a @c FastBitResultSetHandle from a query object.
extern "C" FastBitResultSetHandle
fastbit_build_result_set(FastBitQueryHandle qhandle) {
    FastBitResultSetHandle ret = 0; // a new null handle
    if (qhandle == 0)
	return ret;
    if (qhandle->q.getSelectClause() == 0)
	return ret;
    if (qhandle->q.components().size() == 0)
	return ret;
    if (qhandle->t == 0 ||
	qhandle->q.getState() != ibis::query::FULL_EVALUATE) {
	LOGGER(ibis::gVerbose >= 0)
	    << "Warning -- fastbit_build_result_set -- invalid query "
	    << "handle (" << qhandle << ")";
	return ret;
    }

    ret = new FastBitResultSet;
    ret->results = new ibis::query::result(qhandle->q);
    ret->strbuf.resize(qhandle->q.components().size());
    return ret;
} // fastbit_build_result_set

extern "C" int
fastbit_destroy_result_set(FastBitResultSetHandle rset) {
    delete rset->results;
    delete rset;
    return 0;
} // fastbit_destroy_result_set

extern "C" int
fastbit_result_set_next(FastBitResultSetHandle rset) {
    int ierr;
    if (rset == 0)
	ierr = -2;
    else if (rset->results->next())
	ierr = 0;
    else
	ierr = -1;
    return ierr;
} // fastbit_result_set_next

extern "C" int
fastbit_result_set_get_int(FastBitResultSetHandle rset,
			   const char *cname) {
    int ret = INT_MAX;
    if (rset != 0)
	ret = rset->results->getInt(cname);
    return ret;
} // fastbit_result_set_get_int

extern "C" unsigned
fastbit_result_set_get_unsigned(FastBitResultSetHandle rset,
				const char *cname) {
    unsigned ret = UINT_MAX;
    if (rset != 0)
	ret = rset->results->getUInt(cname);
    return ret;
} // fastbit_result_set_get_unsigned

extern "C" float
fastbit_result_set_get_float(FastBitResultSetHandle rset,
			     const char *cname) {
    float ret = FLT_MAX;
    if (rset != 0)
	ret = rset->results->getFloat(cname);
    return ret;
} // fastbit_result_set_get_float

extern "C" double
fastbit_result_set_get_double(FastBitResultSetHandle rset,
			      const char *cname) {
    double ret = DBL_MAX;
    if (rset != 0)
	ret = rset->results->getDouble(cname);
    return ret;
} // fastbit_result_set_get_double

extern "C" const char*
fastbit_result_set_get_string(FastBitResultSetHandle rset,
			      const char *cname) {
    if (rset == 0)
	return static_cast<const char*>(0);

    unsigned pos = rset->results->colPosition(cname);
    if (pos >= rset->strbuf.size())
	return static_cast<const char*>(0);

    std::string tmp = rset->results->getString(pos);
    rset->strbuf[pos].swap(tmp);
    return rset->strbuf[pos].c_str();
} // fastbit_result_set_get_string

extern "C" int32_t
fastbit_result_set_getInt(FastBitResultSetHandle rset,
			  unsigned pos) {
    int32_t ret = INT_MAX;
    if (rset == 0)
	return ret;
    ret = rset->results->getInt(pos);
    return ret;
} // fastbit_result_set_getInt

extern "C" uint32_t
fastbit_result_set_getUnsigned(FastBitResultSetHandle rset,
			       unsigned pos) {
    uint32_t ret = UINT_MAX;
    if (rset == 0)
	return ret;
    ret = rset->results->getUInt(pos);
    return ret;
} // fastbit_result_set_getUnsigned

extern "C" float
fastbit_result_set_getFloat(FastBitResultSetHandle rset,
			    unsigned pos) {
    float ret = FLT_MAX;
    if (rset == 0)
	return ret;
    ret = rset->results->getFloat(pos);
    return ret;
} // fastbit_result_set_getFloat

extern "C" double
fastbit_result_set_getDouble(FastBitResultSetHandle rset,
			     unsigned pos) {
    double ret = DBL_MAX;
    if (rset == 0)
	return ret;
    ret = rset->results->getDouble(pos);
    return ret;
} // fastbit_result_set_getDouble

extern "C" const char*
fastbit_result_set_getString(FastBitResultSetHandle rset,
			     unsigned pos) {
    if (rset == 0)
	return static_cast<const char*>(0);
    if (pos >= rset->strbuf.size())
	return static_cast<const char*>(0);

    std::string tmp = rset->results->getString(pos);
    rset->strbuf[pos].swap(tmp);
    return rset->strbuf[pos].c_str();
} // fastbit_result_set_getString

/// The new data records are appended to the records already in the
/// directory if there is any.  In addition, if the new records contain
/// columns that are not already in the directory, then the new columns are
/// automatically added with existing records assumed to contain NULL
/// values.  This set of functions are intended for a user to append some
/// number of rows in one operation.  It is clear that writing one row as a
/// time is slow because of the overhead involved in writing the files.  On
/// the other hand, since the new rows are stored in memory, it can not
/// store too many rows.
extern "C" int
fastbit_flush_buffer(const char *dir) {
    int ierr = 0;
    ibis::util::mutexLock
	lock(&ibis::util::envLock, "fastbit_flush_buffer");
    if (dir == 0 || *dir == 0) {
	ierr = -1;
    }
    else if (_capi_tablex != 0) {
	ierr = _capi_tablex->write(dir, 0, 0);
	delete _capi_tablex;
	_capi_tablex = 0;
    }
    else {
	ierr = 0;
    }
    return ierr;
} // fastbit_flush_buffer

/// All invocations of this function are adding data to a single in-memory
/// buffer for a single data partition.
/// @arg colname Name of the column.  Must start with an alphabet and
/// followed by a combination of alphanumerical characters.  Following the
/// SQL standard, the column name is not case sensitive.
/// @arg coltype The type of the values for the column.  The support types
/// are: "double", "float", "long", "int", "short", "byte", "ulong",
/// "uint", "ushort", and "ubyte".  Only the first non-space character is
/// checked for the first six types and only the first two characters are
/// checked for the remaining types.  This string is not case sensitive.
/// @arg vals The array containing the values.  It is expected to contain
/// no less than @c nelem values, though only the first @c nelem values are
/// used by this function.
/// @arg nelem The number of elements of the array @c vals to be added to
/// the in-memory buffer.
/// @arg start The position (row number) of the first element of the
/// array.  Normally, this argument is zero (0) if all values are valid.
/// One may use this argument to skip some rows and indicate to FastBit
/// that the skipped rows contain NULL values.
extern "C" int
fastbit_add_values(const char *colname, const char *coltype,
		   void *vals, uint32_t nelem, uint32_t start) {
    int ierr = -1;
    if (colname == 0 || coltype == 0 || vals == 0) return ierr;
    while (*colname != 0 && isspace(*colname)) ++ colname;
    while (*coltype != 0 && isspace(*coltype)) ++ coltype;
    if (*colname == 0 || *coltype == 0) return ierr;
    if (nelem == 0) return 0;

    ibis::TYPE_T type = ibis::UNKNOWN_TYPE;
    ierr = 0;
    switch (*coltype) {
    default : ierr = -2; break;
    case 'D':
    case 'd': type = ibis::DOUBLE; break;
    case 'F':
    case 'f': type = ibis::FLOAT; break;
    case 'L':
    case 'l': type = ibis::LONG; break;
    case 'I':
    case 'i': type = ibis::INT; break;
    case 'S':
    case 's': type = ibis::SHORT; break;
    case 'B':
    case 'b': type = ibis::BYTE; break;
    case 'U':
    case 'u': {
	switch (coltype[1]) {
	default : ierr = -2; break;
	case 'L':
	case 'l': type = ibis::ULONG; break;
	case 'I':
	case 'i': type = ibis::UINT; break;
	case 'S':
	case 's': type = ibis::USHORT; break;
	case 'B':
	case 'b': type = ibis::UBYTE; break;
	}
	break;}
    }
    if (ierr < 0) return ierr;

    ibis::util::mutexLock
	lock(&ibis::util::envLock, "fastbit_add_values");
    if (_capi_tablex == 0)
	_capi_tablex = new ibis::tafel();
    if (_capi_tablex == 0) return -3;

    ierr = _capi_tablex->addColumn(colname, type);
    ierr = _capi_tablex->append(colname, start, start+nelem, vals);
    return ierr;
} // fastbit_add_values

extern "C" int fastbit_rows_in_partition(const char *dir) {
    int ierr = 0;
    ibis::part *t;
    if (_capi_tlist == 0) {
	fastbit_init(0);
	if (_capi_tlist == 0) {
	    ierr = -1;
	    return ierr;
	}
    }

    t = _capi_tlist->find(dir);
    if (t != 0)
	ierr = t->nRows();
    else
	ierr = -2;
    return ierr;
} // fastbit_rows_in_partition

extern "C" int fastbit_columns_in_partition(const char *dir) {
    int ierr = 0;
    ibis::part *t;
    if (_capi_tlist == 0) {
	fastbit_init(0);
	if (_capi_tlist == 0) {
	    ierr = -1;
	    return ierr;
	}
    }

    t = _capi_tlist->find(dir);
    if (t != 0)
	ierr = t->nColumns();
    else
	ierr = -2;
    return ierr;
} // fastbit_columns_in_partition
