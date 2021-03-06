// Run module
#include "burner.h"
#include "unistd.h"
#include <sys/time.h>
#include "fbaconf.h"

//#define NO_SOUND

extern char gameName[64];

int rom_force_len;

int video_fskipcounter=0;
bool bAltPause = 0;
bool bSoundOn=1;

int bAlwaysDrawFrames = 0;

//bool bShowFPS = true;
int sdl_fps;

int counter;								// General purpose variable used when debugging
extern int nShouldExit;

static int now, done=0;
static float timer = 0,tick=0,  ticks=0;


static unsigned int nNormalLast = 0;		// Last value of timeGetTime()
static int nNormalFrac = 0;					// Extra fraction we did

static bool bAppDoStep = 0;
/*static*/ bool bAppDoFast = 0;
static int nFastSpeed = 10;

static int GetInput(bool bCopy)
{
	static int i = 0;
	InputMake(bCopy); 						// get input
    
	// Update Input dialog ever 3 frames
	if (i == 0) {
		//InpdUpdate();
	}
    
	i++;
    
	if (i >= 3) {
		i = 0;
	}
    
	// Update Input Set dialog
	//InpsUpdate();
	return 0;
}

static void DisplayFPS()
{
	static time_t fpstimer;
	static unsigned int nPreviousFrames;
    
	char fpsstring[8];
	time_t temptime = clock();
	float fps = static_cast<float>(nFramesRendered - nPreviousFrames) * CLOCKS_PER_SEC / (temptime - fpstimer);
	sprintf(fpsstring, "%2.1f", fps);
	VidSNewShortMsg(fpsstring, 0xDFDFFF, 480, 0);
    
	fpstimer = temptime;
	nPreviousFrames = nFramesRendered;
}

// define this function somewhere above RunMessageLoop()
void ToggleLayer(unsigned char thisLayer)
{
	nBurnLayer ^= thisLayer;				// xor with thisLayer
	VidRedraw();
	VidPaint(0);
}

// With or without sound, run one frame.
// If bDraw is true, it's the last frame before we are up to date, and so we should draw the screen
static int RunFrame(int bDraw, int bPause)
{
	static int bPrevPause = 0;
	static int bPrevDraw = 0;
    
	if (bPrevDraw && !bPause) {
		VidPaint(0);							// paint the screen (no need to validate)
	}
    
	if (!bDrvOkay) {
		return 1;
	}
    
	if (bPause) 
	{
		GetInput(false);						// Update burner inputs, but not game inputs
		if (bPause != bPrevPause) 
		{
			VidPaint(2);                        // Redraw the screen (to ensure mode indicators are updated)
		}
	} 
	else 
	{
		nFramesEmulated++;
		nCurrentFrame++;
		GetInput(true);					// Update inputs
	}
	if (bDraw) {
		nFramesRendered++;
		if (VidFrame()) {					// Do one frame
			AudBlankSound();
		}
	} 
	else {								// frame skipping
		pBurnDraw = NULL;					// Make sure no image is drawn
		BurnDrvFrame();
	}
	bPrevPause = bPause;
	bPrevDraw = bDraw;
    
	return 0;
}

static struct timeval start;
void SDL_StartTicks(void) {
    /* Set first ticks value */
    gettimeofday(&start, NULL);
}

unsigned int SDL_GetTicks(void) {
    unsigned int ticks;
    struct timeval now;
    
    gettimeofday(&now, NULL);
    ticks = (now.tv_sec - start.tv_sec) * 1000 + (now.tv_usec - start.tv_usec) / 1000;
    return (ticks);
}



// Callback used when DSound needs more sound
static int RunGetNextSound(int bDraw) {
    float frame_limit = (float)nBurnFPS/100.0f, frametime = 100000.0f/(float)nBurnFPS;
    
	if (nAudNextSound == NULL) {
		return 1;
	}
    
    // Render frame with sound
	pBurnSoundOut = nAudNextSound;
    
	if (bAppDoFast) {  //TURBO MODE
		for (int i = 0; i < nFastSpeed; i++) {
			RunFrame(0, 0);
            memset(nAudNextSound, 0, nAudSegLen << 2);	// Write silence into the buffer
		}
        bDraw=1;
        sdl_fps = 0;
        nFramesRendered = 0;   
        video_fskipcounter=0;
	} else {        
        timer = SDL_GetTicks()/frametime; //track how many frames are drawn
        if(timer-tick>frame_limit && cur_ifba_conf->show_fps) {
            sdl_fps = nFramesRendered;
            nFramesRendered = 0;
            tick = timer;            
        }
    }
    
	//YOYOFR
	RunFrame(bDraw, 0);
    if (bAppDoFast) memset(nAudNextSound, 0, nAudSegLen << 2);		// Write silence into the buffer
    
	return 0;
}




int RunIdle() {
	int nTime, nCount;
    float frame_limit = (float)nBurnFPS/100.0f, frametime = 100000.0f/(float)nBurnFPS;
    
	if (bAudPlaying) {
		// Run with sound
		AudSoundCheck();
		return 0;
	}
    
    
#if BENCH_MODE
    timer = SDL_GetTicks();
    if(timer-tick>1000 && cur_ifba_conf->show_fps) {
        sdl_fps = nFramesRendered;
        nFramesRendered = 0;
        tick = timer;
        //printf("fps:%d\n",sdl_fps);
    }
    now = timer;
    ticks=now-done;
    RunFrame(1,0);
#else
    if (!bAppDoFast) {  //Normal
        timer = SDL_GetTicks()/frametime;
        if(timer-tick>frame_limit && cur_ifba_conf->show_fps) {
            sdl_fps = nFramesRendered;
            nFramesRendered = 0;
            tick = timer;
            //printf("fps:%d\n",fps);
        }
        now = timer;
        ticks=now-done;
        
        if(ticks<1) { //TO FAST, Limit rendering speed
            usleep(100); //0.1ms
            return 0;
        }
        
        if (cur_ifba_conf->video_fskip==10) {//AUTO FSKIP        
            if(ticks>10) ticks=10;
            for (int i=0; i<ticks-1; i++) {
                RunFrame(0,0);	
            }
            RunFrame(1,0);
        } else {
            video_fskipcounter++;
            if (video_fskipcounter>cur_ifba_conf->video_fskip) {
                video_fskipcounter=0;
                RunFrame(1,0);
            } else RunFrame(0,0);
            
        }
    } else {  //TURBO
        for (int i=0;i<10;i++) RunFrame(0,0);
        timer = SDL_GetTicks()/frametime;
        sdl_fps = 0;
        nFramesRendered=0;
        tick = timer;
        now = timer;
        ticks=now-done;
        video_fskipcounter=0;
        RunFrame(1,0);
    }
#endif            
    
    done = now;
    
    return 0;
}

int RunReset()
{
	// Reset the speed throttling code
	nNormalLast = 0; nNormalFrac = 0;
	if (!bAudPlaying) {
		// run without sound
		nNormalLast = SDL_GetTicks();
	}
	return 0;
}

static int RunInit()
{
	// Try to run with sound
    if (bSoundOn) {
        AudSetCallback(RunGetNextSound);
        AudSoundPlay();
    } else {
        bAudOkay=0;
        AudSetCallback(NULL);
    }
	RunReset();
    
	return 0;
}

static int RunExit()
{
	nNormalLast = 0;
	// Stop sound if it was playing
    if (bSoundOn) AudSoundStop();
	return 0;
}

// The main message loop
int RunMessageLoop()
{
	int bRestartVideo;
	int finished= 0;
    
    video_fskipcounter=0;
    SDL_StartTicks();
    
	do {
		bRestartVideo = 0;
        
		//MediaInit();
        
		if (!bVidOkay) {
            
			// Reinit the video plugin
			VidInit();
			if (!bVidOkay && nVidFullscreen) {
                
				nVidFullscreen = 0;
				VidInit();
			}
            
        }
        
		RunInit();
        
		GameInpCheckMouse();															// Hide the cursor
        
        done=0;timer = 0;ticks=0;tick=0;sdl_fps = 0;
		while (!finished) {
            if (nShouldExit==1) {
                finished=1;                
            }
            if (nShouldExit==0) {
				RunIdle();
			} else {
                usleep(10000); //10ms
            }
		}
		RunExit();        
	} while (bRestartVideo);
    
	return 0;
}

/////////////////////////////////////////
/* DUMMY FUNC for IOS port */
/////////////////////////////////////////
void NeoCDInfo_Exit() {
    
}
// for NeoGeo CD (WAV playback)
void wav_pause(bool bResume) {
    
}