#include <stdlib.h>
#include <stdio.h>
#include <string.h>


#include "General_GUI_S.h"
#include "InitLights.h"
#include "sgeComm.h"

using namespace std ;


//
//
////////////////////////////////////////////////////////////////////////




///////////////////////////////////////////////////////////////////////////
//
// Callbacks
//

#define ALL_COLOR_COMPONENTS (-1)

static void
setLMColor (int whichComponent, double val)
	{

	double clampedVal = CLAMP01(val);

        if (whichComponent == ALL_COLOR_COMPONENTS)
                {
                LightModel.ambient[0] =
                LightModel.ambient[1] =
                LightModel.ambient[2] = val;

                LightModelWidgets.rgbInput[0]->value(clampedVal);
                LightModelWidgets.rgbInput[1]->value(clampedVal);
                LightModelWidgets.rgbInput[2]->value(clampedVal);
                LightModelWidgets.grayInput->value(clampedVal);
                }
        else
                {
                LightModel.ambient[whichComponent] = val;
                LightModelWidgets.rgbInput[whichComponent]->value(clampedVal);
                }
    


	}  // end of setLMColor


static void localViewerCB (Fl_Widget *w)
	{
	LightModel.localViewer = LightModelWidgets.localViewer->value (); 
	sgeSendLightModelLocalViewer ();
	}  // end of localViewerCB


static void twoSidedCB (Fl_Widget *w)
	{
	LightModel.twoSided = LightModelWidgets.twoSided->value (); 
	sgeSendLightModelTwoSided ();
	}  // end of twoSidedCB


static void colorControlCB (Fl_Widget *w)
	{
	LightModel.colorControlSeparate = 
		LightModelWidgets.colorControl->value (); 
	sgeSendLightModelColorControl ();
	}  // end of colorControlCB



static void lscolorModeCB (Fl_Widget *w)
	{
	int inRgbMode = ((Fl_Light_Button *)w)->value();

	// printf ("lscolorModeCB %d\n", inRgbMode);

	if ( LightModel.inRgbMode != inRgbMode )
		{

		if (inRgbMode)
			{
			// switch from gray to rgb mode
			// Data should be OK, so I'm not going to do anything

			}
		else
			{
			// switch from rgb to gray mode
			double sum = 0;
			for (int j = 0; j < 3; j++)
				{
				sum += LightModel.ambient[j];
				}
			sum /= 3;
			for (int j = 0; j < 3; j++)
				{
				LightModel.ambient[j] = sum;
				}

			}

		LightModel.inRgbMode = inRgbMode;
		reconcileGuiToLightModel ();
		}

	}  // end of lscolorModeCB



static void lightModelResetCB (Fl_Widget *w)
        {
        setLightModelDefaults ();
        sgeSendLightModel ();
	reconcileGuiToLightModel ();
        }  // end of lightModelResetCB



static void scaleLMColor (
				int whichComponent, 
				double factor,
				double defaultValue )
	{
	int i, j, startCol, endCol, startComp, endComp;
	double minVal;
	char rgbStr[10];
	int doGray = 0;;

	// minVal = (factor == 0) ? 0 : 0.001;
	minVal = 0.0;


	// factor = MAX (factor, 0.001);



	if (whichComponent == ALL_COLOR_COMPONENTS)
		{
		doGray = 1;
		startComp = 0;
		endComp = 2;
		}
	else
		{
		startComp = endComp = whichComponent;
		}


		double sum = 0;
		for (j = startComp; j <= endComp; j++)
		    {
		    // if factor is non-neg then use it as a factor
		    if (factor > 0.0)
			{
		    	LightModel.ambient[j] *= factor;
			}
		    else if (factor == 0.0)
			{
			// this is a bit of a kludge
#define COLOR_SCALED_TO_ZERO (0.00000013)
			if (LightModel.ambient[j] != 0)
			    {
		    	    LightModel.ambient[j] = 
							COLOR_SCALED_TO_ZERO;
			    }
			}
		    else
			{
			// if factor is negative, then it's a signal that
			// we were unable to calculate a factor, so we should
			// either leave the color unchanged if it's not
			// the value that signals that it had been scaled to
			// zero, or set it to the supplied defaultValue.
			// We do this because when a color has been scaled
			// to zero, we want to raise it back up when we
			// are scaling up in tandem with another value that
			// has started at zero.  But if it became zero not by
			// scaling, then it should stay at zero.
			// this is  all pretty kludgey.
			if (LightModel.ambient[j] == 
							COLOR_SCALED_TO_ZERO)
				{
		    		LightModel.ambient[j] = 
								defaultValue;
				}

			}

		    sum += LightModel.ambient[j];
		



		    LightModelWidgets.rgbInput[j]->
		      value (CLAMP01(LightModel.ambient[j]));
		    }

		LightModelWidgets.grayInput->value(CLAMP01(sum/3.0));

			
	}  // end of scaleLMColor






static void floatInputCB (Fl_Widget *w, long id)
	{
	int i, j;
	Fl_Value_Input *fltIn = (Fl_Value_Input *) w;
	Fl_Scrollbar *sb;
	double val;


	val = fltIn->value();

	// printf ("NEW!!!  floatInputCB : id = %d val = %f\n", id, val);

	i = id / 100;  // which color: amb, diff, spec
	j = id % 100;  // which component, r, g, b


	if (j == 3)
		{
		// we're in gray mode 
		setLMColor (ALL_COLOR_COMPONENTS, val);
		sb = LightModelWidgets.grayScrollbar;
		}
	else
		{
		// we're in rgb mode
		setLMColor (j, val);
		if ( LightModel.inRgbMode ) 
			{
			sb = LightModelWidgets.rgbScrollbar[j];
			}
		else
			{
			sb = NULL;
			}
		}

	// printf ("sb = %p\n", sb);

	if (sb != NULL)
		{
		// printf ("sb value = %ld\n", sb->value());
		if ( (sb->value() / 1000.0) != val)
			{
			((Fl_Valuator *)sb)->value (1000*val);
			}
		}

	sgeSendLightModelColor ();

	}  // end of floatInputCB


static void scrollbarCB (Fl_Widget *w, long id)
	{
	Fl_Value_Input *fltIn;
	char rgbStr[10];
	double val = (((Fl_Scrollbar *)w)->value ())/1000.0;

	//printf ("scrollbarCB : %d   %d\n", id, ((Fl_Scrollbar *)w)->value ());
	if ( LightModel.inRgbMode ) 
		{
		setLMColor (id, val);
		}
	else
		{
		setLMColor (ALL_COLOR_COMPONENTS, val);
		}

	
	sgeSendLightModelColor ();
	}  // end of scrollbarCB



static void scaleLMRGBCB (Fl_Widget *w)
	{
	double val, factor, ratio;
	static double lastVal = 1.0;
	int icolor;

	// printf ("scaleLMRGBCB: value = %f\n", ((Fl_Roller *)w)->value());

	if (Fl::event () == FL_RELEASE)
		{
		((Fl_Roller *)w)->value(1.0);
		lastVal = 1.0;
		return;
		}

	val = ((Fl_Roller *)w)->value();
	ratio = val/lastVal;
	factor = 1.0 - (10*(1.0 - ratio));

	// printf ("( val %f  / lastVal %f )  = ratio %f and    factor = %f\n", 
		// val, lastVal, ratio, factor);

	lastVal = val;

		
	scaleLMColor (ALL_COLOR_COMPONENTS, factor, 0.001);

	sgeSendLightModelColor ();




	int j;
	double sum = 0;
	for (j = 0; j < 3; j++)
		{ 
		sum += LightModel.ambient[j];
		}
	sum /= 3;
	((Fl_Valuator *)LightModelWidgets.grayScrollbar)->value (1000*sum);

	((Fl_Valuator *)LightModelWidgets.rgbScrollbar[0])->
			  value (1000*LightModel.ambient[0]);
	((Fl_Valuator *)LightModelWidgets.rgbScrollbar[1])->
			  value (1000*LightModel.ambient[1]);
	((Fl_Valuator *)LightModelWidgets.rgbScrollbar[2])->
			  value (1000*LightModel.ambient[2]);



	}  // end of scaleLMRGBCB



//
// End of callbacks
//
///////////////////////////////////////////////////////////////////////////
//
// Build the GUI
//






static Fl_Group *buildOtherLMColorEditGroup (int x, int y, int w, int h)
	{
	int currX, currY;
	

	// printf ("buildOtherLMColorEditGroup %d %d %d %d\n", x, y, w, h);

	Fl_Group *rtnGroup = new Fl_Group (x, y, w, h);
	rtnGroup->end();


	currX = x+10;

	currY = y+8;

        Fl_Button *resetButton = 
		new Fl_Button (currX+12, currY, 130, 30, "Reset");
	resetButton->callback (lightModelResetCB);
	rtnGroup->add (resetButton);


	// currY += 56;
	currY += 64;
	Fl_Light_Button *colorEditModeButton = 
		new Fl_Light_Button (currX+12, currY, 130, 60,
		"Edit R, G, and B \nIndependently");
	colorEditModeButton->callback (lscolorModeCB);

	// currY += 74;
	currY += 96;
	Fl_Roller *scaleRGB = new Fl_Roller (currX, currY, 390-(x+10), 20, 
						"Scale R, G, and B\ntogether");
	scaleRGB->type (FL_HORIZONTAL);
	scaleRGB->range (0.0, 2.0);

	scaleRGB->callback (scaleLMRGBCB);
	scaleRGB->step (0.0001);
	// scaleRGB->range (2.0, 0.0);
	scaleRGB->value (1.0);
	scaleRGB->when (FL_WHEN_RELEASE_ALWAYS | FL_WHEN_CHANGED);


	LightModelWidgets.scaleRGB = scaleRGB;

	LightModelWidgets.rgbEditMode = colorEditModeButton;

	rtnGroup->add (LightModelWidgets.rgbEditMode);
	rtnGroup->add (scaleRGB);



#if 0
Need to add three buttons:
	* enable / disable local viewer calculations
	* enable / disable two sided surface rendering
	* enable / disable separate specular color calculation
#endif


	// rtnGroup->hide ();

	return rtnGroup;
	}  // end of buildOtherLMColorEditGroup




static Fl_Group *buildGrayEditGroup (int x, int y, int w, int h)
	{
	int subGrpX, subGrpY;

	int height = MIN_V_LINE_HEIGHT + 3;
	int spacing = MIN_V_LINE_SPACING + 3;

	Fl_Group *rtnGroup = new Fl_Group (x, y, w, h);
	rtnGroup->end();
	rtnGroup->hide ();


	// Fl_Box *label = new Fl_Box (x+10, y+60, 50, 30, "Gray Edit");
	// rtnGroup->add (label);

        Fl_Box *label = new Fl_Box (x+100, y+6, 80, height, "brightness");
        rtnGroup->add (label);



	subGrpX = x;
	subGrpX = x + 1;
        subGrpY = y;

	// AMBIENT
        subGrpY += spacing;

	Fl_Box *ambBox = 
		new Fl_Box (subGrpX+5, subGrpY, 70, height, "Ambient");
	rtnGroup->add (ambBox);

        // Fl_Group *ambGroup = new Fl_Group (subGrpX, subGrpY, 230, height);
        Fl_Group *ambGroup = new Fl_Group (subGrpX+2, subGrpY-3, 182, height+6);
        ambGroup->end();
	Fl_Value_Input *ambInput =
                new Fl_Value_Input (subGrpX+100, subGrpY, 80, height);
	ambInput->range (0.0, 1.0);
	ambInput->step (0.001);
	ambInput->value (0.0);
	ambGroup->add (ambInput);

	rtnGroup->add (ambGroup);


	// ambButton->callback (colorButtonCB, 0);

	LightModelWidgets.grayColorGroup = ambGroup;

	LightModelWidgets.grayInput = ambInput;

	LightModelWidgets.grayInput->callback (floatInputCB, 003);

	

	// Now the slider:

	subGrpY += spacing + 5;
	// height = 390-subGrpY;
	height = (y+h-10)-subGrpY;
// fprintf (stderr, "LM height = %d\n", height);

	Fl_Scrollbar *grayScrollbar = 
		new Fl_Scrollbar (subGrpX+110+20,  subGrpY, 20, height);
	LightModelWidgets.grayScrollbar = grayScrollbar;
	grayScrollbar->callback (scrollbarCB, 3);

        grayScrollbar->bounds (1000.0, 0.0);
        grayScrollbar->step(10.0);
        grayScrollbar->linesize (1);

	rtnGroup->add (grayScrollbar);


        Fl_Box *zeroLabel =
                new Fl_Box (subGrpX+95, subGrpY+height-43, 30, 40, "0.0");
        Fl_Box *oneLabel =
                new Fl_Box (subGrpX+95, subGrpY+6, 30, 40, "1.0");
        // printf ("Scrollbar subGrpY = %d  height = %d\n", subGrpY, height);

        rtnGroup->add (zeroLabel);
        rtnGroup->add (oneLabel);

	return rtnGroup;
	}  // end of buildGrayEditGroup



static Fl_Group *buildLMParamsGroup (int x, int y, int w, int h)
	{
	Fl_Group *rtnGroup = new Fl_Group (x, y, w, h);
	rtnGroup->end();

	int spacing = 40;

	int buttonWidth = (w-3*spacing)/3;


	int currY = y + 10;

	int currX = spacing / 2;



	LightModelWidgets.twoSided= 
		new Fl_Light_Button (currX, currY, buttonWidth, 60,
		" Two\n Sided");
	LightModelWidgets.twoSided->callback (twoSidedCB);
	LightModelWidgets.twoSided->
	  tooltip ("Light both sides of polygons.");


	currX += buttonWidth + spacing;


	LightModelWidgets.localViewer = 
		new Fl_Light_Button (currX, currY, buttonWidth, 60,
		" Local\n Viewer");
	LightModelWidgets.localViewer->callback (localViewerCB);
	LightModelWidgets.localViewer->
	  tooltip ("Calculate specular highlights\nwith a local viewpoint.");


	currX += buttonWidth + spacing;


	LightModelWidgets.colorControl = 
		new Fl_Light_Button (currX, currY, buttonWidth, 60,
		" Separate\n Specular");
	LightModelWidgets.colorControl->callback (colorControlCB);
	LightModelWidgets.colorControl->
	  tooltip ("Calculate specular color separately.");

	rtnGroup->add (LightModelWidgets.localViewer);
	rtnGroup->add (LightModelWidgets.twoSided);
	rtnGroup->add (LightModelWidgets.colorControl);





#if 0
Need to add three buttons:
	* enable / disable local viewer calculations
	* enable / disable two sided surface rendering
	* enable / disable separate specular color calculation
#endif



	return rtnGroup;
	}  // end of buildLMParamsGroup


static Fl_Group *buildRgbEditGroup (int x, int y, int w, int h)
	{
	int subGrpX, subGrpY;
	int height, spacing;
	int i, j;

	height = MIN_V_LINE_HEIGHT + 3;
	spacing = MIN_V_LINE_SPACING + 3;
	// height = STD_V_LINE_HEIGHT;
	// spacing = STD_V_LINE_SPACING;

	Fl_Group *rtnGroup = new Fl_Group (x, y, w, h);
	rtnGroup->end();

	Fl_Box *rLabel = new Fl_Box (x+80, y+6, 47, height, "red");
	rtnGroup->add (rLabel);
	Fl_Box *gLabel = new Fl_Box (x+130, y+6, 47, height, "green");
	rtnGroup->add (gLabel);
	Fl_Box *bLabel = new Fl_Box (x+180, y+6, 47, height, "blue");
	rtnGroup->add (bLabel);


	// subGrpX = x;
	subGrpX = x + 1;
	subGrpY = y+spacing;
	// Fl_Group *ambGroup = new Fl_Group (subGrpX, subGrpY, 230, height);
	Fl_Group *ambGroup = 
		new Fl_Group (subGrpX+2, subGrpY-3, 230-2, height+6);
	ambGroup->end();
	LightModelWidgets.rgbColorGroup = ambGroup;
	Fl_Box *ambBox = 
		new Fl_Box (subGrpX+5, subGrpY, 70, height, "Ambient");
	rtnGroup->add (ambBox);
	// ambButton->callback (colorButtonCB, 0);
	Fl_Value_Input *ambR = 
		new Fl_Value_Input (subGrpX+80, subGrpY, 47, height);
	ambR->range (0.0, 1.0);
	ambR->step (0.001);
	LightModelWidgets.rgbInput[0] = ambR;
	ambR->value (0.0);
	ambGroup->add (ambR);
	Fl_Value_Input *ambG = 
		new Fl_Value_Input (subGrpX+130, subGrpY, 47, height);
	ambG->range (0.0, 1.0);
	ambG->step (0.001);
	LightModelWidgets.rgbInput[1] = ambG;
	ambG->value (0.1);
	ambGroup->add (ambG);
	Fl_Value_Input *ambB = 
		new Fl_Value_Input (subGrpX+180, subGrpY, 47, height);
	ambB->range (0.0, 1.0);
	ambB->step (0.001);
	LightModelWidgets.rgbInput[2] = ambB;
	ambB->value (0.2);
	ambGroup->add (ambB);
	rtnGroup->add (ambGroup);

	for (j = 0; j < 3; j++)
		{
		LightModelWidgets.rgbInput[j]->
				callback (floatInputCB, 100*i + j);
		}

	subGrpX = x;
	subGrpY += spacing + 5;

	// height = 390-subGrpY;
	height = (y+h-10)-subGrpY;
	
	// printf ("Scrollbar subGrpY = %d  height = %d\n", subGrpY, height);

	Fl_Group *sliderGroup = 
		new Fl_Group (subGrpX, subGrpY, 230, height);
	sliderGroup->end();

	rtnGroup->add (sliderGroup);


	Fl_Scrollbar *rScrollbar = 
		new Fl_Scrollbar (subGrpX+80+15,  subGrpY, 20, height);
	LightModelWidgets.rgbScrollbar[0] = rScrollbar;
	rScrollbar->bounds (1000.0, 0.0);
	rScrollbar->step(10.0);
	rScrollbar->linesize (1);
	Fl_Scrollbar *gScrollbar = 
		new Fl_Scrollbar (subGrpX+130+15, subGrpY, 20, height);
	LightModelWidgets.rgbScrollbar[1] = gScrollbar;
	gScrollbar->bounds (1000.0, 0.0);
	gScrollbar->step(10.0);
	gScrollbar->linesize (1);
	Fl_Scrollbar *bScrollbar = 
		new Fl_Scrollbar (subGrpX+180+15, subGrpY, 20, height);
	LightModelWidgets.rgbScrollbar[2] = bScrollbar;
	bScrollbar->bounds (1000.0, 0.0);
	bScrollbar->step(10.0);
	bScrollbar->linesize (1);

	rScrollbar->callback (scrollbarCB, 0);
	gScrollbar->callback (scrollbarCB, 1);
	bScrollbar->callback (scrollbarCB, 2);


	rScrollbar->when (FL_WHEN_CHANGED);
	gScrollbar->when (FL_WHEN_CHANGED);
	bScrollbar->when (FL_WHEN_CHANGED);

	sliderGroup->add (rScrollbar);
	sliderGroup->add (gScrollbar);
	sliderGroup->add (bScrollbar);


	Fl_Box *zeroLabel = 
		new Fl_Box (subGrpX+60, subGrpY+height-43, 30, 40, "0.0");
	Fl_Box *oneLabel = 
		new Fl_Box (subGrpX+60, subGrpY+6, 30, 40, "1.0");
	// printf ("Scrollbar subGrpY = %d  height = %d\n", subGrpY, height);

	sliderGroup->add (zeroLabel);
	sliderGroup->add (oneLabel);

	return rtnGroup;
	}  // buildRgbEditGroup


static Fl_Group *buildLMColorEditGroup (int x, int y, int w, int h)
	{

	Fl_Group *rtnGroup = new Fl_Group (x, y, w, h);
	rtnGroup->end();

	// rtnGroup->box (FL_SHADOW_BOX);

	int currY = y;

	Fl_Box *label = new Fl_Box (x+10, currY+10, 200, 30, 
						"Edit Light Model");
	label->labelfont (FL_HELVETICA_ITALIC);
	label->labelsize (18);
	label->align (FL_ALIGN_INSIDE | FL_ALIGN_LEFT);
	rtnGroup->add (label);

	currY += 20;

	Fl_Group *otherBox = 
		buildOtherLMColorEditGroup (231, currY, 170, h-145);
	rtnGroup->add (otherBox);

	currY += 20;

	Fl_Group *rgbBox = buildRgbEditGroup (x, currY, 230, h-145);
	rtnGroup->add (rgbBox);
	LightModelWidgets.rgbEditGroup = rgbBox;
	
	Fl_Group *grayBox = buildGrayEditGroup (x, currY, 230, h-145);
	rtnGroup->add (grayBox);
	grayBox->hide ();
	LightModelWidgets.grayEditGroup = grayBox;

	currY += h - 145;

	Fl_Group *paramsGrp = buildLMParamsGroup (x, currY, w, y+h-currY);
	rtnGroup->add (paramsGrp);

	return rtnGroup;
	

	}  // end of buildLMColorEditGroup


//
//
// End of building of the GUI
//
/////////////////////////////////////////////////////////////////////


Fl_Group *buildModelPanel (int x, int y, int w, int h)
	{
	Fl_Group *modelGroup = new Fl_Group (x, y, w, h, "Light Model");
	modelGroup->end();

#if 0
	// Fl_Box *label = new Fl_Box (x+10, y+60, 250, 60, 
	Fl_Box *label = new Fl_Box (x, y, w, h/2, 
			"Light Model Panel\nUnder Construction");
	modelGroup->add (label);
#else

	Fl_Group *colorGroup = buildLMColorEditGroup (x, y, w, h);
	modelGroup->add (colorGroup);

#endif

	return modelGroup;
	}  // end of buildModelPanel


