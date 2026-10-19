#ifndef GUI_HANDHELD_TEXT_H
#define GUI_HANDHELD_TEXT_H

namespace HandheldText
{
    // Reflow authored PC line breaks using the font's measured handheld width.
    // Keep every non-whitespace character, including words wider than one line.
    // A return value of 17 means the caller must retry with a smaller font.
    template<class Character, class Fits>
    unsigned Wrap(const Character* text, Character (&lines)[16][256], Fits fits)
    {
        unsigned row=0;
        while( text && *text )
        {
            while( *text==' ' || *text=='\n' || *text=='\r' ) ++text;
            if( !*text ) break;
            if( row==16 ) return 17;
            unsigned length=0,wordBreak=0;
            const Character* nextWord=nullptr;
            Character* line=lines[row];
            while( *text && length<255 )
            {
                const Character c=(*text=='\n' || *text=='\r') ? ' ' : *text;
                line[length++]=c;
                line[length]=0;
                if( !fits(line) && length>1 )
                {
                    --length;
                    if( nextWord )
                    {
                        text=nextWord;
                        length=wordBreak;
                    }
                    break;
                }
                ++text;
                if( c==' ' )
                {
                    wordBreak=length-1;
                    nextWord=text;
                }
            }
            while( length && line[length-1]==' ' ) --length;
            line[length]=0;
            ++row;
        }
        return row;
    }
}
#endif
