// File: $Id$
// Author: John Wu <John.Wu at ACM.org>
// Copyright 2007-2009 the Regents of the University of California
#ifndef IBIS_TAFEL_H
#define IBIS_TAFEL_H
#include "table.h"	// ibis::table
#include "bitvector.h"	// ibis::bitvector

///@file
/// An expandable table.  This file defines ibis::tafel.

namespace ibis {
    class tafel;
}

/// An expandable table.  It inherents from ibis::tablex only, therefore
/// does not support any querying functions.  It stores all its content in
/// memory before the function write is called, therefore, it can only
/// handle relatively small number of rows.
///
///@note The word tafel is a German word for table.
class ibis::tafel : public ibis::tablex {
public:
    tafel() : mrows(0U) {}
    virtual ~tafel() {clear();}

    virtual int addColumn(const char* cname, ibis::TYPE_T ctype,
			  const char* cdesc, const char* idx);
    virtual int SQLCreateTable(const char *stmt, std::string&);

    virtual int append(const char* cname, uint64_t begin, uint64_t end,
		       void* values);
    virtual int appendRow(const ibis::table::row&);
    virtual int appendRow(const char*, const char*);
    virtual int appendRows(const std::vector<ibis::table::row>&);
    virtual int readCSV(const char* filename, int maxrows,
			const char* outputdir, const char* delimiters);
    virtual int readSQLDump(const char* filename, std::string& tname,
			    int maxrows, const char* outputdir);

    virtual int write(const char* dir, const char* tname,
		      const char* tdesc, const char* idx) const;
    virtual int writeMetaData(const char* dir, const char* tname,
			      const char* tdesc, const char* idx) const;

    virtual void clearData();
    virtual int32_t reserveSpace(uint32_t);
    virtual uint32_t capacity() const;

    virtual uint32_t mRows() const {return mrows;}
    virtual uint32_t mColumns() const {return cols.size();}
    virtual void describe(std::ostream&) const;

    /// In-memory version of a column.
    struct column {
	/// Name of the column.
	std::string name;
	/// Description of the column.
	std::string desc;
	/// Index specification for the column
	std::string indexSpec;
	/// Type of the data.
	ibis::TYPE_T type;
	/// Pointer to the in-memory storage.  For fix-sized elements, this
	/// is a pointer to an array_t object.  For null-terminated
	/// strings, this is a pointer to std::vector<std::string>.  For
	/// binary objects, it is the raw bytes in these objects (type
	/// array_t<unsigned char>.  The starting positions of the objects
	/// are stored in starts.
	void* values;
	/// Starting positions of the binary objects stored in values.
	ibis::array_t<int64_t> starts;
	/// The default value for the column.  SQL standard allows a column
	/// to take on a default value if it is not explicitly specified.
	/// For fix-sized elements, this variable points to the default of
	/// the given type.  For string values, this is a pointer to a
	/// std::string.  If this variable is nil, the unspecified values
	/// will be marked as null values through the variable mask.
	void* defval;
	/// Valid values are marked 1, null values are marked 0.
	ibis::bitvector mask;

	column();
	~column();
    }; // column
    typedef std::map<const char*, column*, ibis::lessi> columnList;
    /// The list of columns stored in memory.
    const columnList& getColumns() const {return cols;}

protected:
    /// List of columns in alphabetical order.
    columnList cols;
    /// Order of columns as they were specified through @c addColumn.
    std::vector<column*> colorder;
    /// Number of rows of this table.
    ibis::bitvector::word_t mrows;

    /// Clear all content.  Removes both data and metadata.
    void clear();

    /// Make all short columns catch up with the longest one.
    void normalize();

    template <typename T>
    void append(const T* in, ibis::bitvector::word_t be,
		ibis::bitvector::word_t en, array_t<T>& out,
		const T& fill, ibis::bitvector& mask) const;
    void appendString(const std::vector<std::string>* in,
		      ibis::bitvector::word_t be,
		      ibis::bitvector::word_t en,
		      std::vector<std::string>& out,
		      ibis::bitvector& mask) const;
    void appendRaw(const ibis::array_t<unsigned char>* in,
		   ibis::bitvector::word_t be,
		   ibis::bitvector::word_t en,
		   std::vector<std::string>& out,
		   ibis::bitvector& mask) const;

    template <typename T>
    void locate(ibis::TYPE_T, std::vector<array_t<T>*>& buf,
		std::vector<ibis::bitvector*>& msk) const;
    void locateString(ibis::TYPE_T t,
		      std::vector<std::vector<std::string>*>& buf,
		      std::vector<ibis::bitvector*>& msk) const;
    template <typename T>
    void append(const std::vector<std::string>& nm, const std::vector<T>& va,
		std::vector<array_t<T>*>& buf,
		std::vector<ibis::bitvector*>& msk);
    void appendString(const std::vector<std::string>& nm,
		      const std::vector<std::string>& va,
		      std::vector<std::vector<std::string>*>& buf,
		      std::vector<ibis::bitvector*>& msk);
    int parseLine(const char* str, const char* del, const char* id);

    template <typename T>
    int writeColumn(int fdes, ibis::bitvector::word_t nold,
		    ibis::bitvector::word_t nnew,
		    const array_t<T>& vals, const T& fill,
		    ibis::bitvector& totmask,
		    const ibis::bitvector& newmask) const;
    int writeString(int fdes, ibis::bitvector::word_t nold,
		    ibis::bitvector::word_t nnew,
		    const std::vector<std::string>& vals,
		    ibis::bitvector& totmask,
		    const ibis::bitvector& newmask) const;
    int writeRaw(int bdes, int sdes, ibis::bitvector::word_t nold,
		 ibis::bitvector::word_t nnew,
		 const ibis::array_t<unsigned char>& bytes,
		 const ibis::array_t<int64_t>& starts,
		 ibis::bitvector& totmask,
		 const ibis::bitvector& newmask) const;

    int32_t doReserve(uint32_t);
    int assignDefaultValue(ibis::tafel::column &col, const char *val) const;
    int readSQLStatement(std::istream &, ibis::fileManager::buffer<char>&,
			 ibis::fileManager::buffer<char>&) const;
    uint32_t preferredSize() const;

private:
    tafel(const tafel&);
    tafel& operator=(const tafel&);
}; // class ibis::tafel
#endif // IBIS_TAFEL_H
