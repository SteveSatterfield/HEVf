#include <iris.h>

// this file is generated by the makefile from the fluid file
#include "hev-presenterGUI.cxx"

////////////////////////////////////////////////////////////////////////
int main(int argc, char **argv)
{
    window = NULL ;

    // send dtkMsg files to stderr
    // to get INFO level messages set the envvar DTK_SPEW=info
    dtkMsg.setFile(stderr) ;

    // set the signls that kill us off
    iris::Signal(signalCatcher) ;
    
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

    if (fileNumber == 0)
    {
	dtkMsg.add(DTKMSG_ERROR, "hev-presenter: no files to display!\n") ;
	return 1 ;
    }

    ////////////////
    // create the main window
    window = make_window() ;
    window->end();
    window->show(fltk_argc, fltk_argv);

    // disable killing the window using the close button or escape button
    if (noescape) window->callback(doNothingCB);

    // set the maximum value in the valuator
    slideNumberValuator->maximum(slides.size()-1) ;

    // set default movie control values
    movieLoopButton->value(defaultLoopValue) ;
    movieAutoStartButton->value(defaultAutoStartValue) ;
    movieFPSValuator->value(defaultFPSValue) ;

    // initially the movie group is inactive
    movieGroup->deactivate() ;
    
    // create the hev-imageDisplay process
    if (!createImageDisplay()) return 1 ;

    //if (debug()) printSlides() ;

    // make the auto-play button invisible if no PLAY commands were ever encountered
    if (!anyAutoPlay) 
    {
	autoPlaying = false ;
	autoPlayButton->hide() ;
    }
    else
    {
	autoPlaying = true ;
	autoPlayButton->value(autoPlaying) ;
    }

    firstSlide() ;

    // the GUI loop
    while (Fl::wait() && running)
    {
	
	// if you exit the main window, stop running the application.
	// since we disabled the escape key and window close button this
	// really shouldn't be possible, but you never know...
	if (!window->shown()) running = false ;

    };

    cleanup() ;

    return 0 ;
}


