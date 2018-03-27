#include <signal.h>
#include "hev-shapeSelectionFunctions.cpp"
#include "hev-shapeSelectionGUI.cxx"

static bool running = true ;

////////////////////////////////////////////////////////////////////////
// for catching the KILL, QUIT, ABRT, TERM and INT signals
void signal_catcher(int sig)
{
    fprintf(stderr,"PID %d caught signal %d, starting exit sequence ...\n", getpid(), sig);
    running = false ;
    // reset the signals
    iris::Signal(SIG_DFL); 
}

////////////////////////////////////////////////////////////////////////
int main(int argc, char **argv)
{
    
    // catch signals that kill us off
    iris::Signal(signal_catcher) ;
    
    // send messages to stderr
    dtkMsg.setFile(stderr) ;

    ////////////////
    // this stuff is for command parsing
    if(dtkFLTKOptions_get(argc, (const char **) argv,
			       &fltk_argc,  &fltk_argv,
			       &other_argc, &other_argv)) 
    {
	usage() ;
	return 1 ;
    }
    
    // parse the local arguments and do other setup
    if(!setup(other_argc, other_argv))
    {
	usage() ;
	return 1 ;
    }
    
    //fprintf(stderr,"before creating window and running GUI\n") ;
    
    ////////////////
    // start up the fltk GUI and run in a loop
    main_window = make_main_window() ;
    main_window->end();
    main_window->show(fltk_argc, fltk_argv);
    
    make_objectDescription_window() ;
    objectDescription_window->end() ;
    objectDescription_window->show(fltk_argc, fltk_argv);

    // disable killing the window using the close button or escape button
    if (noEscape) 
      {
	main_window->callback(doNothingCB);
	objectDescription_window->callback(doNothingCB);
      }

    while (Fl::wait() && running)
    {
	// if you exit the main window, stop running the application
	if (!main_window->shown()) running = false ;
	
    };

    return 0 ;
}


