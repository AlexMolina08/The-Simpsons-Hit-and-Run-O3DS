#ifndef HANDHELD_CREDITS_H
#define HANDHELD_CREDITS_H

namespace HandheldCredits
{
    const float RoleY=52.0f, NameY=75.0f, OriginalY=126.0f;
    const float Height=14.0f, Advance=19.0f, PartitionGap=19.0f, Width=272.0f;

    // Stream complete authored lines through one small scratch buffer. The same
    // walker measures the roll and draws it, so wrapping cannot change its span.
    template<class Character,class Fits,class Emit>
    unsigned Walk(const Character* text,Fits fits,Emit emit)
    {
        unsigned rows=0;
        if(!text)return rows;
        Character line[128];
        while(*text)
        {
            if(*text=='\n' || *text=='\\'){
                line[0]=0;emit(line,rows++);++text;continue;
            }
            const Character* end=text;
            unsigned length=0,lastSpace=0;
            while(*end && *end!='\n' && *end!='\\' && length<127)
            {
                line[length]=*end;line[length+1]=0;
                if(length && !fits(line))break;
                if(*end==' ')lastSpace=length;
                ++length;++end;
            }
            if(*end && *end!='\n' && *end!='\\' && lastSpace){
                length=lastSpace;end=text+lastSpace+1;
            }
            line[length]=0;emit(line,rows++);text=end;
            if(*text=='\n' || *text=='\\')++text;
        }
        return rows;
    }
}
#endif
