// generated by Fast Light User Interface Designer (fluid) version 1.0109

#include "showCursorPositionInImageGUI.h"
#include "showCursorPositionInImageFunctions.h"

Fl_Double_Window *main_window=(Fl_Double_Window *)0;

Fl_Light_Button *imageButton=(Fl_Light_Button *)0;

static void cb_imageButton(Fl_Light_Button* o, void*) {
  image(bool(o->value()));
}

Fl_Double_Window* make_main_window() {
  { main_window = new Fl_Double_Window(85, 30);
    { Fl_Light_Button* o = imageButton = new Fl_Light_Button(5, 5, 75, 20, "image");
      imageButton->callback((Fl_Callback*)cb_imageButton);
      imageButton = o ;
      o->value(imageState) ;
    } // Fl_Light_Button* imageButton
    main_window->end();
  } // Fl_Double_Window* main_window
  return main_window;
}

Fl_Double_Window *sub_window=(Fl_Double_Window *)0;

Fl_Box *image_box=(Fl_Box *)0;

Fl_Double_Window* make_sub_window() {
  { sub_window = new Fl_Double_Window(400, 300);
    { image_box = new Fl_Box(25, 25, 35, 17);
      image_box->when(FL_WHEN_CHANGED);
    } // Fl_Box* image_box
    sub_window->end();
  } // Fl_Double_Window* sub_window
  return sub_window;
}
