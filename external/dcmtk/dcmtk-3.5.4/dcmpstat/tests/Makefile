#
#	Makefile for dcmpstat/tests
#




SHELL = /bin/sh
VPATH = ../include:../../config/include
srcdir = .
top_srcdir = ..
configdir = ../../config

include $(configdir)/Makefile.def

ofstddir = $(top_srcdir)/../ofstd
dcmdatadir = $(top_srcdir)/../dcmdata
dcmnetdir = $(top_srcdir)/../dcmnet
dcmqrdbdir = $(top_srcdir)/../dcmqrdb
dcmimgledir = $(top_srcdir)/../dcmimgle
dcmsigndir = $(top_srcdir)/../dcmsign
dcmtlsdir = $(top_srcdir)/../dcmtls
dcmsrdir = $(top_srcdir)/../dcmsr

LOCALINCLUDES = -I$(dcmnetdir)/include -I$(ofstddir)/include -I$(dcmdatadir)/include \
	-I$(dcmsigndir)/include -I$(dcmtlsdir)/include -I$(dcmqrdbdir)/include \
	-I$(dcmimgledir)/include -I$(dcmsrdir)/include
LIBDIRS = -L$(top_srcdir)/libsrc -L$(dcmnetdir)/libsrc -L$(ofstddir)/libsrc \
	-L$(dcmdatadir)/libsrc -L$(dcmsigndir)/libsrc -L$(dcmtlsdir)/libsrc \
	-L$(dcmqrdbdir)/libsrc -L$(dcmimgledir)/libsrc -L$(dcmsrdir)/libsrc 
LOCALLIBS = -ldcmpstat -ldcmdsig -ldcmimgle -ldcmsr -ldcmqrdb -ldcmnet -ldcmtls \
	-ldcmdata -lofstd $(XMLLIBS) $(ZLIBLIBS) $(TCPWRAPPERLIBS) $(OPENSSLLIBS)

objs = msgserv.o
progs = msgserv
support = philips.lut


all: $(progs)

msgserv: msgserv.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(LIBDIRS) -o $@ msgserv.o $(LOCALLIBS) $(MATHLIBS) $(LIBS)


install: all install-support

install-support:
	$(configdir)/mkinstalldirs $(datadir)
	for file in $(support); do \
		$(INSTALL_DATA) $$file $(datadir) ;\
	done

clean:
	rm -f $(objs) $(progs) $(TRASH)

distclean:
	rm -f $(objs) $(progs) $(DISTTRASH)


dependencies:
	$(CXX) -MM $(defines) $(includes) $(CPPFLAGS) $(CXXFLAGS) *.cc  > $(DEP)

include $(DEP)
