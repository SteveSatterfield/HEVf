/*
 to test in home directory using 2 terminal windows on same system:

 in both terminal windows:
   PATH=".:$PATH"
   MCP_FILE_PATH=".:$MCP_FILE_PATH"
   OSG_FILE_PATH=".:$OSG_FILE_PATH"

 in terminal window 2:
   export DTK_SHAREDMEM_DIR=/dev/shm/dtkSharedMem_collabfly
   export DTK_PORT=34220
   export DTK_CONNECTION=localhost:$DTK_PORT
   export IRIS_CONTROL_FIFO=/tmp/irisControlFifo-kelso_collabfly
   export IRISFLY_MCP_CONTROL_FIFO=/tmp/irisfly-mcp-fifo-kelso_collabfly

 in terminal window 1:
   make && collabfly --m cube :34220 irisfly --examine cone.osg sphere.osg cube.osg 

 in terminal window 2:
   collabfly --m cube : irisfly --examine cone.osg sphere.osg cube.osg

 the first graphics window to come up will be for terminal window 2

*/

extern Fl_Double_Window *main_window;

// separate out the fltk arguments from the others
int fltk_argc;
char **fltk_argv;
int other_argc;
char **other_argv;

// if anything change it to false when it's time to exit
bool running = true ;

// ignore escapes and close window buttons
void doNothingCB(Fl_Widget*, void*) { } ;

// true if disabling ESCAPE and the window manager close button
bool noEscape = false ;

bool followRemoteLocationValue = false ;
#ifdef WAND
bool followRemoteWandValue = false ;
#endif
#ifdef HEAD
bool followRemoteHeadValue = false ;
#endif

// where to write the local location and wand- all matrices in world coordinates
std::string localLocationShmName ;
dtkSharedMem* localLocationShm ;
osg::Matrix localLocation ;

std::string localWandShmName ;
dtkSharedMem* localWandShm ;
osg::Matrix localWand ;

// where to read the remote location and wand- all matrices in world coordinates
std::string remoteLocationShmName ;
dtkSharedMem* remoteLocationShm ;
osg::Matrix remoteLocation ;
osg::Matrix oldRemoteLocation ;

std::string remoteWandShmName ;
dtkSharedMem* remoteWandShm ;
osg::Matrix remoteWand ;
osg::Matrix oldRemoteWand ;

// nodes to display remote wands. remote wands are under the world node. local wands are just hung under scene node
std::string remoteWandNode = "hev-collab-remoteWandShmMatrix" ;
std::string localWandNode = "hev-collab-localWandShmMatrix" ;

// wand nodes
std::vector<std::string> remoteWandObjectNodes ;
std::vector<std::string> remoteWandObjectFilenames ;

std::vector<std::string> localWandObjectNodes ;
std::vector<std::string> localWandObjectFilenames ;

int currentRemoteWand = 0 ;
int currentLocalWand = 0 ;

// where to read the button that indicates the local wand should move
std::string buttonShmName = "idea/buttons/left" ;
dtkSharedMem* buttonShm;
char button = 0 ;

// where to read the local wand location in scene coordinates
std::string wandShmName = "idea/offsetWand" ;
dtkSharedMem* wandShm;
osg::Matrix wand ;
osg::Matrix oldWand ;

// this is a byte shared memory to tell the SHMMATRIX node whether to update the local wand
std::string localWandOnOffShmName = "collabfly/localWandOnOff" ;
dtkSharedMem* localWandOnOffShm;
char localWandOnOff = 0 ;

// the selector string- only do anything when it's set
std::string selectorString = "collab" ;
// and the shared memory that sets it
std::string selectorStringShmName = "idea/selector" ;
iris::ShmString* selectorStringShm ;
bool selected = false ;

// the location is read from the IRIS state
iris::ShmState shmState ;
dtkSharedMem* locationShm ;
osg::Matrix location ;
osg::Matrix oldLocation ;
osg::Matrix locationInv ;

// the world is also read from the IRIS state
dtkSharedMem* worldShm ;
osg::Matrix world ;
osg::Matrix oldWorld ;
osg::Matrix worldInv ;

// name of the node to be moved
// if blank, nothing to move
std::string moverNodeName ;
// also passed in are the names of the matrix data and who is moving the node
std::string moverMatrixShmName ;
dtkSharedMem* moverMatrixShm ;
std::string moverOwnerShmName ;
dtkSharedMem* moverOwnerShm ;
char* moverOwnerShmValue ;
char* oldMoverOwnerShmValue ;
char* hostname ;

// the mover selector string
std::string moverSelectorString = "collab-objectMover" ;
bool moverSelected = false ;

// and the name of the matrix node that will change with shm
std::string moverMatrixNodeName = "hev-collab-shmMatrix" ;

// just store the state of the move button
bool moveButtonValue = false ;

// the name of the scenegraph node that is set by the mover
std::string moverWandNodeName = "worldOffsetWand" ;

// the hev-relativeMove command and the fifo used to RESET it
std::string relativeMoveFifoName ;
std::string relativeMoveCmd ;

// how long to sleep
int ticks = iris::GetUsleep() ;

////////////////////////////////////////////////////////////////////////
// get an existing shared memory file
bool getSharedMem(std::string& name, dtkSharedMem** shm, size_t size = 0, bool create=false)
{
    int flag ;
    if (create) flag = DTK_GET ;
    else flag = DTK_CONNECT ;

    // ignore size if not specified
    if (size == 0)
    {
	*shm = new dtkSharedMem(name.c_str(), flag) ;
    }
    else
    {
	*shm = new dtkSharedMem(size, name.c_str(), flag) ;
    }
    
    if ((*shm)->isInvalid()) return false ;
    return true ;
}

////////////////////////////////////////////////////////////////////////
// called by the signal catcher set in main()
void cleanup(int s)
{
    dtkMsg.add(DTKMSG_DEBUG, "hev-collab::cleanup: signal %d caught in cleanup! setting running to false\n",s) ;
    running = false ;
}

////////////////////////////////////////////////////////////////////////
// see if we can load the file, and if so, sniff it
bool init(int argc, char **argv)
{
    int c = 1 ;
    //~fprintf(stderr,("argc = %d\n",argc) ;
    while (c<argc && argv[c][0] == '-' && argv[c][1] == '-')
    {
	//~fprintf(stderr,("c = %d, argv[%d] = %s\n",c,c,argv[c]) ;
	if (iris::IsSubstring("--noescape",argv[c],3))
	{
	    noEscape = true ;
	    //~fprintf(stderr,("setting noescape\n") ;
	    c++ ;
	}
	
	else if (iris::IsSubstring("--usleep",argv[c],3)) 
	{
	    c++ ;
	    if (c<argc || !(iris::StringToInt(argv[c], &ticks)) || ticks<0) return false ;
	    c++ ;
	}

	else if (iris::IsSubstring("--localLocationShmName",argv[c],8))
	{
	    c++ ;
	    if (c<argc)
	    {
		localLocationShmName = argv[c] ;
		c++ ;
	    }
	}

	else if (iris::IsSubstring("--localWandShmName",argv[c],8))
	{
	    c++ ;
	    if (c<argc)
	    {
		localWandShmName = argv[c] ;
		c++ ;
	    }
	}

 	else if (iris::IsSubstring("--remoteLocationShmName",argv[c],8))
	{
	    c++ ;
	    if (c<argc)
	    {
		remoteLocationShmName = argv[c] ;
		c++ ;
	    }
	}

 	else if (iris::IsSubstring("--remoteWandShmName",argv[c],8))
	{
	    c++ ;
	    if (c<argc)
	    {
		remoteWandShmName = argv[c] ;
		c++ ;
	    }
	}

	else if (iris::IsSubstring("--move",argv[c],3))
	{
	    c++ ;
	    //fprintf(stderr,"c = %d, argc = %d\n",c,argc) ;
	    if (c+2<argc)
	    {
		moverNodeName = argv[c] ;
		c++ ;
		moverMatrixShmName = argv[c] ;
		c++ ;
		moverOwnerShmName = argv[c] ;
		c++ ;
	    }
	}

	// !!! ADD PARAMS FOR SELECTOR STING, SELECTOR SHM, BUTTON SHM, OTHERS...

	else return false ;
    }

    // these must already exist, and they must be specified on the command line
    if (localLocationShmName == "" || !getSharedMem(localLocationShmName, &localLocationShm, sizeof(double)*16)) return false ;
    if (localWandShmName == "" || !getSharedMem(localWandShmName, &localWandShm, sizeof(double)*16)) return false ;
    if (remoteLocationShmName == "" || !getSharedMem(remoteLocationShmName, &remoteLocationShm, sizeof(double)*16)) return false ;
    if (remoteWandShmName == "" || !getSharedMem(remoteWandShmName, &remoteWandShm, sizeof(double)*16)) return false ;

    // these have defaults, but must already exist
    if (!getSharedMem(buttonShmName, &buttonShm, 1)) return false ;
    if (!getSharedMem(wandShmName, &wandShm, 128)) return false ;

    // this one we can create if needed but it has to have the right size
    if (!getSharedMem(localWandOnOffShmName, &localWandOnOffShm, 1, true)) return false ;
    localWandOnOffShm->write(&localWandOnOff) ;

    // I guess these are too, they come from the shared memory state
    std::map<std::string, iris::ShmState::DataElement>::iterator pos = shmState.getMap()->find("nav") ;
    if (pos == shmState.getMap()->end()) return false ;
    iris::ShmState::DataElement de = pos->second ;
    if (de.type != iris::ShmState::MATRIX) return false ;
    locationShm = de.shm ;
    if (locationShm->isInvalid()) return false ;
    pos = shmState.getMap()->find("world") ;
    if (pos == shmState.getMap()->end()) return false ;
    de = pos->second ;
    if (de.type != iris::ShmState::MATRIX) return false ;
    worldShm = de.shm ;
    if (worldShm->isInvalid()) return false ;

    // and this one's special because it's a std::string
    selectorStringShm = new iris::ShmString(selectorStringShmName) ;
    if (selectorStringShm->isInvalid()) return false ;

    // set up mover stuff if --move was specified
    if (moverNodeName.size() != 0)
    {
	osg::Matrix moverMatrix ;
	if (!getSharedMem(moverMatrixShmName, &moverMatrixShm, 16*sizeof(double))) return false ;
	moverMatrixShm->write(moverMatrix.ptr()) ;

	if (!getSharedMem(moverOwnerShmName, &moverOwnerShm)) return false ;
	moverOwnerShmValue = (char*) malloc(moverOwnerShm->getSize()) ;
	oldMoverOwnerShmValue = (char*) malloc(moverOwnerShm->getSize()) ;
	hostname = (char*) malloc(moverOwnerShm->getSize()) ;
	memset(moverOwnerShmValue, 0, moverOwnerShm->getSize()) ;
	memset(oldMoverOwnerShmValue, 0, moverOwnerShm->getSize()) ;
	moverOwnerShm->write(moverOwnerShmValue) ;
	gethostname(hostname, moverOwnerShm->getSize()) ;
	// add pid to end of hostname string so it'll be unique if running two copies on same system
	std::string pid = "-" + iris::IntToString(getpid()) ;
	strcat(hostname, pid.c_str()) ;

	relativeMoveFifoName = "/tmp/hev-collab-"+std::string(getenv("USER"))+iris::IntToString(getpid())+"-relativeMoveFifo" ;

	relativeMoveCmd = "EXEC hev-relativeMove --inTRButtonShm " + buttonShmName + " --inShm " +  localWandShmName + " --selectorStr " + moverSelectorString + " --outShm " + moverMatrixShmName  + " --fifo " + relativeMoveFifoName ;

	// start hev-relativeMove
	printf("%s\n", relativeMoveCmd.c_str()) ;
	
	// get a list of the parents of the node to be moved
	std::string parentsFifoName("/tmp/hev-collab-"+std::string(getenv("USER"))+"-parentsFifo") ;
	iris::FifoReader parentsFifo(parentsFifoName) ;
	parentsFifo.open() ;
	parentsFifo.unlinkOnExit() ;
	printf("QUERY %s PARENTS %s\n",parentsFifoName.c_str(), moverNodeName.c_str()) ;
	fflush(stdout) ;
	std::string line ;
	while (1)
	{
	    if (parentsFifo.readLine(&line)) break ;
	    usleep(iris::GetUsleep()) ;
	}

	std::vector<std::string> parents = iris::ParseString(line) ;
	// node not found
	int n ;
	if (!iris::StringToInt(parents[2],&n)) return false ;
	// no parents
	if (parents.size()<4) return false ;
	// more than one parent
	if (n>1) return false ;
	// parent not world node
	if (parents[3] != "world") return false ;

	// hook up the mover node
	printf("SHMMATRIX %s %s\n", moverMatrixNodeName.c_str(), moverMatrixShmName.c_str()) ;
	printf("ADDCHILD %s world\n",moverMatrixNodeName.c_str()) ;
	// hook up the matrix node
	// put the node to be moved under it
	printf("REMOVECHILD %s world\n",moverNodeName.c_str()) ;
	printf("ADDCHILD %s %s\n", moverNodeName.c_str(), moverMatrixNodeName.c_str()) ;
	fflush(stdout) ;

    }

    //fprintf(stderr,"%s: %d\n",__FILE__,__LINE__) ;

    return true ;
}

////////////////////////////////////////////////////////////////////////
// every program should have one
void usage()
{
    fprintf(stderr,"Usage: hev-collab [ --noescape ] ...\n") ;
}

////////////////////////////////////////////////////////////////////////
// set the location based on the value in the remoteLocation matrix
// this value is in world coordinates, so need to convert to scene
void setLocation()
{
    remoteLocationShm->read(remoteLocation.ptr()) ;
    if (remoteLocation != oldRemoteLocation)
    {
	//fprintf(stderr,"remoteLocation changed\n") ;
	oldRemoteLocation = remoteLocation ;
	osg::Matrix sceneRemoteLocation = remoteLocation * worldInv ;
	//iris::PrintMatrix(sceneRemoteLocation,stderr) ;
	printf("NAV MATRIX %f %f %f %f  %f %f %f %f  %f %f %f %f  %f %f %f %f\n",
	       sceneRemoteLocation(0,0), sceneRemoteLocation(0,1), sceneRemoteLocation(0,2), sceneRemoteLocation(0,3),
	       sceneRemoteLocation(1,0), sceneRemoteLocation(1,1), sceneRemoteLocation(1,2), sceneRemoteLocation(1,3),
	       sceneRemoteLocation(2,0), sceneRemoteLocation(2,1), sceneRemoteLocation(2,2), sceneRemoteLocation(2,3),
	       sceneRemoteLocation(3,0), sceneRemoteLocation(3,1), sceneRemoteLocation(3,2), sceneRemoteLocation(3,3)) ;
	fflush(stdout) ;
    }
}

////////////////////////////////////////////////////////////////////////
// load wands under world node
void loadWands()
{
    // can't seem to get fluid to do this for me
    noneLocalWandMenuItem->set() ;
    noneRemoteWandMenuItem->set() ;
    
    // the names of the 5 remote wands
    remoteWandObjectFilenames.push_back("") ;
    remoteWandObjectFilenames.push_back("bluePointer.osg") ;
    remoteWandObjectFilenames.push_back("blueRay.osg") ;
    remoteWandObjectFilenames.push_back("blueIcon.osg") ;
    remoteWandObjectFilenames.push_back("blueAxis.osg") ;
    remoteWandObjectFilenames.push_back("blueDot.osg") ;

    // the name of the node for each wand
    remoteWandObjectNodes.push_back("") ;
    for (unsigned int i=1; i<remoteWandObjectFilenames.size(); i++)
    {
	remoteWandObjectNodes.push_back("hev-collab-remoteWand-" + remoteWandObjectFilenames[i]) ;
    }

    // the names of the 5 local wands
    localWandObjectFilenames.push_back("") ;
    localWandObjectFilenames.push_back("greenPointer.osg") ;
    localWandObjectFilenames.push_back("greenRay.osg") ;
    localWandObjectFilenames.push_back("greenIcon.osg") ;
    localWandObjectFilenames.push_back("greenAxis.osg") ;
    localWandObjectFilenames.push_back("greenDot.osg") ;

    // the name of the node for each wand
    localWandObjectNodes.push_back("") ;
    for (unsigned int i=1; i<localWandObjectFilenames.size(); i++)
    {
	localWandObjectNodes.push_back("hev-collab-localWand-" + localWandObjectFilenames[i]) ;
    }

    // the matrix is updated remotely and automatically in the scenegraph
    printf("SHMMATRIX %s %s\nNOCLIP %s\nADDCHILD %s world\n",remoteWandNode.c_str(), remoteWandShmName.c_str(), remoteWandNode.c_str(), remoteWandNode.c_str()) ;
    // load wand objects
    for (unsigned int i=1; i<remoteWandObjectFilenames.size(); i++)
    {
	printf("LOAD %s %s\nNODEMASK %s OFF\nADDCHILD %s %s\n", remoteWandObjectNodes[i].c_str(), remoteWandObjectFilenames[i].c_str(), remoteWandObjectNodes[i].c_str(), remoteWandObjectNodes[i].c_str(), remoteWandNode.c_str()) ;
    }

    // the local wand is under the scene node
    printf("SHMMATRIX %s %s %s\nNOCLIP %s\nADDCHILD %s scene\n",localWandNode.c_str(), wandShmName.c_str(), localWandOnOffShmName.c_str(), localWandNode.c_str(), localWandNode.c_str()) ;
    printf("AFTER CLEANUP EXEC dtk-destroySharedMem %s\n",localWandOnOffShmName.c_str()) ;
    // load wand objects
    for (unsigned int i=1; i<localWandObjectFilenames.size(); i++)
    {
	printf("LOAD %s %s\nNODEMASK %s OFF\nADDCHILD %s %s\n", localWandObjectNodes[i].c_str(), localWandObjectFilenames[i].c_str(), localWandObjectNodes[i].c_str(), localWandObjectNodes[i].c_str(), localWandNode.c_str()) ;
    }

    fflush(stdout) ;

}

////////////////////////////////////////////////////////////////////////
void moveWithWand()
{
    //fprintf(stderr,"moveWithWand\n") ;

    // turn off navigation
    printf("NAV ACTIVE OFF\n") ;
    fflush(stdout) ;

    activeOutput->color(::fl_rgb_color(150, 255, 150)) ;
    activeOutput->redraw() ;
    activeOutput->value("            active") ;

}

////////////////////////////////////////////////////////////////////////
void moveWithMatrix()
{
    //fprintf(stderr,"moveWithMatrix\n") ;
    
    // turn on navigation
    printf("NAV ACTIVE ON\n") ;
    fflush(stdout) ;

    activeOutput->color(::fl_rgb_color(255, 150, 150)) ;
    activeOutput->redraw() ;
    activeOutput->value("          inactive") ;
}

////////////////////////////////////////////////////////////////////////
// do this every "frame"
void process()
{
    static bool first = true ;

    if (first) moveWithMatrix() ;

    // read the wand button
    buttonShm->read(&button) ;

    // has the transformation in the world node changed?
    worldShm->read(world.ptr()) ;
    bool worldChanged ;
    if (world != oldWorld)
    {
	//fprintf(stderr,"world changed\n") ;
	oldWorld = world ;
	worldChanged = true ;
	worldInv.invert(world) ;
	//iris::PrintMatrix(world,stderr) ;
    }
    else worldChanged = false ;

    // notice if our location and wand have changed
    locationShm->read(location.ptr()) ;
    bool locationChanged ;
    if (location != oldLocation)
    {
	//fprintf(stderr,"location changed\n") ;
	oldLocation = location ;
	locationChanged = true ;
	locationInv.invert(location) ;
	//iris::PrintMatrix(location,stderr) ;
    }
    else locationChanged = false ;
    
    // update changes to world location
    if (first || worldChanged || locationChanged)
    {
	//fprintf(stderr,"writing localLocation: %s:\n",localLocationShmName.c_str()) ;
	localLocation = location * world ;
	//iris::PrintMatrix(localLocation,stderr) ;
	localLocationShm->write(localLocation.ptr()) ;
    }

    // and update the remote wand - they're in world coords
    remoteWandShm->read(remoteWand.ptr()) ;
    if (remoteWand != oldRemoteWand)
    {
	//fprintf(stderr,"remoteWand changed\n") ;
	oldRemoteWand = remoteWand ;
	//iris::PrintMatrix(remoteWand,stderr) ;
    }

    // and shared location if following
    if (followRemoteLocationValue)
    {
	setLocation() ;	
    }

    // and the wand
    wandShm->read(wand.ptr()) ;
    bool wandChanged ;
    if (wand != oldWand)
    {
	//fprintf(stderr,"wand changed\n") ;
	oldWand = wand ;
	wandChanged = true ;
    }
    else wandChanged = false ;
    
    if (first || worldChanged || locationChanged || wandChanged)
    {
	//fprintf(stderr,"writing localWand\n") ;
	localWand = wand * locationInv * worldInv ;
	//iris::PrintMatrix(localWand,stderr) ;
	localWandShm->write(localWand.ptr()) ;
    }

    // are we active?
    if (!selected && selectorStringShm->getString() == selectorString)
    {
	//fprintf(stderr,"collab is now active\n") ;
	selected = true ;
    }
    if (selected && selectorStringShm->getString() != selectorString)
    {
	//fprintf(stderr,"collab is now inactive\n") ;
	selected = false ;
    }

    localWandOnOff = 0 ;
    if (selected && button) localWandOnOff = 1 ;
    else localWandOnOff = 0 ;

    localWandOnOffShm->write(&localWandOnOff) ;

    if (moverNodeName.size() != 0)
    {
	// look for changes in the owner shared memory
	moverOwnerShm->read(moverOwnerShmValue) ;

	// if the owner changed
	if (strcmp(oldMoverOwnerShmValue, moverOwnerShmValue) != 0)
	{
	    //fprintf(stderr,"moverOwnerShmValue = \"%s\"\n",moverOwnerShmValue) ;
	    //fprintf(stderr,"oldMoverOwnerShmValue = \"%s\"\n",oldMoverOwnerShmValue) ;

	    //it changed to blank, so anybody can use it
	    if (strlen(moverOwnerShmValue) == 0) 
	    {
		//fprintf(stderr,"activating moveButton\n") ;	
		moveButton->activate() ;
	    }

	    //it changed to something other than our hostname, so we can't use it
	    else if (strcmp(moverOwnerShmValue, hostname) != 0) 
	    {
		moveButton->deactivate() ;
		//fprintf(stderr,"deactivating moveButton\n") ;	
	    }

	    strcpy(oldMoverOwnerShmValue,moverOwnerShmValue) ; 
	}

	static bool movingWithMatrix = true ; // call movingWithMatrix() first time through
	bool movingWithWand = moverSelected && moveButtonValue && button ;
	if (movingWithWand && movingWithMatrix) 
	{
	    moveWithWand() ;
	    movingWithMatrix = false ;
	}
	else if (!movingWithWand && !movingWithMatrix)
	{
	    moveWithMatrix() ;
	    movingWithMatrix = true ;
	}
    }

    first = false ;

}

////////////////////////////////////////////////////////////////////////
void copyRemoteLocation()
{
    dtkMsg.add(DTKMSG_INFO, "hev-collab::copyRemoteLocation\n") ;
    setLocation() ;
}

////////////////////////////////////////////////////////////////////////
void followRemoteLocation(bool onOff)
{
    if (onOff == followRemoteLocationValue) return ;
    followRemoteLocationValue = onOff ;

    if (onOff) copyRemoteLocationButton->deactivate() ;
    else copyRemoteLocationButton->activate() ;

    dtkMsg.add(DTKMSG_INFO, "hev-collab::followRemoteLocation(%d)\n",onOff) ;
}

#ifdef WAND
////////////////////////////////////////////////////////////////////////
void copyRemoteWand()
{
    dtkMsg.add(DTKMSG_INFO, "hev-collab::copyRemoteWand\n") ;
}

////////////////////////////////////////////////////////////////////////
void followRemoteWand(bool onOff)
{
    if (onOff == followRemoteWandValue) return ;
    followRemoteWandValue = onOff ;

    if (onOff) copyRemoteWandButton->deactivate() ;
    else copyRemoteWandButton->activate() ;

    dtkMsg.add(DTKMSG_INFO, "hev-collab::followRemoteWand(%d)\n",onOff) ;
}
#else
void copyRemoteWand() { ; } ;
void followRemoteWand(bool onOff) { ; } ;
#endif

#ifdef HEAD
////////////////////////////////////////////////////////////////////////
void copyRemoteHead()
{
    dtkMsg.add(DTKMSG_INFO, "hev-collab::copyRemoteHead\n") ;
}

////////////////////////////////////////////////////////////////////////
void followRemoteHead(bool onOff)
{
    if (onOff == followRemoteHeadValue) return ;
    followRemoteHeadValue = onOff ;

    if (onOff) copyRemoteHeadButton->deactivate() ;
    else copyRemoteHeadButton->activate() ;

    dtkMsg.add(DTKMSG_INFO, "hev-collab::followRemoteHead(%d)\n",onOff) ;
}
#else
void copyRemoteHead() { ; } ;
void followRemoteHead(bool onOff) { ; } ;
#endif

////////////////////////////////////////////////////////////////////////
void localWandType(int i)
{
    if (i==currentLocalWand) return ;

    dtkMsg.add(DTKMSG_INFO, "hev-collab::localWandType(%d)\n",i) ;

    if (i==0)
    {
	printf("NODEMASK %s OFF\n",localWandObjectNodes[currentLocalWand].c_str()) ;
    }
    else
    {
	if (currentLocalWand!=0) printf("NODEMASK %s OFF\n",localWandObjectNodes[currentLocalWand].c_str()) ;
	printf("NODEMASK %s ON\n",localWandObjectNodes[i].c_str()) ;
    }
    fflush(stdout) ;
    currentLocalWand = i ;
}

////////////////////////////////////////////////////////////////////////
void remoteWandType(int i)
{
    if (i==currentRemoteWand) return ;

    dtkMsg.add(DTKMSG_INFO, "hev-collab::remoteWandType(%d)\n",i) ;

    if (i==0)
    {
	printf("NODEMASK %s OFF\n",remoteWandObjectNodes[currentRemoteWand].c_str()) ;
    }
    else
    {
	if (currentRemoteWand!=0) printf("NODEMASK %s OFF\n",remoteWandObjectNodes[currentRemoteWand].c_str()) ;
	printf("NODEMASK %s ON\n",remoteWandObjectNodes[i].c_str()) ;
    }
    fflush(stdout) ;
    currentRemoteWand = i ;
}

#ifdef HEAD
////////////////////////////////////////////////////////////////////////
void localHeadType(int i)
{
    dtkMsg.add(DTKMSG_INFO, "hev-collab::localHeadType(%d)\n",i) ;
}

////////////////////////////////////////////////////////////////////////
void remoteHeadType(int i)
{
    dtkMsg.add(DTKMSG_INFO, "hev-collab::remoteHeadType(%d)\n",i) ;
}
#else
void localHeadType(int i) { ; } ;
void remoteHeadType(int i) { ; } ;
#endif

////////////////////////////////////////////////////////////////////////
// set the "owner"
void moveCallback(bool v)
{
    moveButtonValue = v ;
    if (v)
    {
	strcpy(moverOwnerShmValue, hostname) ;
	
	//fprintf(stderr,"move button pressed, moverOwnerShmValue = \"%s\"\n",moverOwnerShmValue) ;	

	// this will disable the pointer and navigation updates until we're done
	selectorStringShm->setString(moverSelectorString) ;
	moverSelected = true ;

	// disable the other menu items
	//remoteLocationGroup->deactivate() ;
	//wandTypeGroup->deactivate() ;

	// reset hev-relativeMove
	osg::Matrix moverMat ;
	moverMatrixShm->read(moverMat.ptr()) ;
	dtkCoord moverCoord = iris::MatrixToCoord(moverMat) ;
	std::string reset = "WAIT echo RESET " + 
	    iris::FloatToString(moverCoord.x) + " " +  
	    iris::FloatToString(moverCoord.y) + " " +  
	    iris::FloatToString(moverCoord.z) + " " +  
	    iris::FloatToString(moverCoord.h) + " " +  
	    iris::FloatToString(moverCoord.p) + " " +  
	    iris::FloatToString(moverCoord.r) + " > " + relativeMoveFifoName ;
	printf("%s\n",reset.c_str()) ;
	fflush(stdout) ;
	//fprintf(stderr,"resetting relativeMove: %s\n", reset.c_str()) ;

    }
    else
    {
	char blank[1] ;
	blank[0] = 0 ;
	strcpy(moverOwnerShmValue, blank) ;
	//fprintf(stderr,"move button released, moverOwnerShmValue = \"%s\"\n",moverOwnerShmValue) ;	

	// enable the other menu items
	//remoteLocationGroup->activate() ;
	//wandTypeGroup->activate() ;

	// this will enable the pointer and navigation updates until we're done
	selectorStringShm->setString(selectorString) ;
	moverSelected = false ;

    }
    moverOwnerShm->write(moverOwnerShmValue) ;
}


