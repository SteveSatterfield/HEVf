#include <iris.h>

// this file is generated by the makefile from the fluid file
#include "hev-wiggleNavGUI.cxx"

////////////////////////////////////////////////////////////////////////
int main(int argc, char **argv)
{
    
    // send dtkMsg files to stderr
    // to get INFO level messages set the envvar DTK_SPEW=info
    dtkMsg.setFile(stderr) ;

    // set the signls that kill us off
    iris::Signal(cleanup) ;
    
    ////////////////
    // this stuff is for command parsing- it pulls out the fltk arguments
    // and leaves the rest for our program
    if(dtkFLTKOptions_get(argc, (const char **) argv,
			    &fltk_argc,  &fltk_argv,
			    &other_argc, &other_argv)) 
    {
	usage() ;
	return 1 ;
    }
    
    // parse the local arguments and do other setup
    if(! init(other_argc, other_argv))
    {
	usage() ;
	return 1 ;
    }
    
    ////////////////
    // create the main window
    Fl_Double_Window* main_window = make_main_window() ;
    main_window->end();
    main_window->show(fltk_argc, fltk_argv);

    // disable killing the window using the close button or escape button
    if (noEscape) main_window->callback(doNothingCB);

    Fl::add_timeout(1.0/freq, timer_callback) ;
    // the GUI loop
    while (Fl::wait() && running)
    {
    };

    return 0 ;
}


