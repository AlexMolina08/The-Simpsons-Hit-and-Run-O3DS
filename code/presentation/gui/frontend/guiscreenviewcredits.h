//===========================================================================
// Copyright (C) 2003 Radical Entertainment Ltd.  All rights reserved.
//
// Component:   CGuiScreenViewCredits
//
// Description: 
//              
//
// Authors:     Tony Chu
//
// Revisions		Date			Author	    Revision
//                  2003/05/14      TChu        Created for SRR2
//
//===========================================================================

#ifndef GUISCREENVIEWCREDITS_H
#define GUISCREENVIEWCREDITS_H

//===========================================================================
// Nested Includes
//===========================================================================
#include <presentation/gui/guiscreen.h>

//===========================================================================
// Forward References
//===========================================================================

//===========================================================================
// Interface Definitions
//===========================================================================
class CGuiScreenViewCredits : public CGuiScreen
{
public:
    CGuiScreenViewCredits( Scrooby::Screen* pScreen, CGuiEntity* pParent );
    virtual ~CGuiScreenViewCredits();

	virtual void HandleMessage( eGuiMessage message, 
			                    unsigned int param1 = 0,
								unsigned int param2 = 0 );

#ifdef RAD_3DS
    Scrooby::Text* GetCreditsText3DS(unsigned index) const;
    unsigned GetNumCreditsTexts3DS() const { return MAX_NUM_CREDITS_PARTITIONS; }
    float GetCreditsScroll3DS() const { return m_currentTranslateY*0.5f; }
#endif

protected:
    void InitIntro();
	void InitRunning();
	void InitOutro();

    void ResetScrolling();
    bool m_playKKDialog : 1;

    Scrooby::Group* m_creditsGroup;

private:
    float FormatTextWithLineBreaks( Scrooby::Text* pText );

    bool ScrollCredits( float pixels );
    virtual void OnScrollingDone();

    void OnNewLineDisplayed( int lineNumber );

    static const int MAX_NUM_CREDITS_PARTITIONS = 34;

    static float s_totalTranslateY;
    static float s_numPixelsPerLine;
    static float s_creditsOffsets[ MAX_NUM_CREDITS_PARTITIONS ];

    Scrooby::Text* m_credits[ MAX_NUM_CREDITS_PARTITIONS ];
    float m_currentTranslateY;
    int m_lastLineDisplayed;

    static const unsigned int SCROLL_IDLE_TIME_BEFORE_RESET = 0; // in msec
    unsigned int m_elapsedIdleTime;

};

#endif // GUISCREENVIEWCREDITS_H
