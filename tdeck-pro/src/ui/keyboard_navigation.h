#pragma once
#include <algorithm>
namespace mm {
// Move within the actual grid, without wrapping across row edges.
inline int moveAppFocus(int focus,char key,int count,int columns){
 if(count<=0||columns<=0)return -1;
 if(focus<0||focus>=count)return 0;
 int next=focus;
 if(key=='w')next=focus-columns;
 else if(key=='s')next=focus+columns;
 else if(key=='a'&&focus%columns)next=focus-1;
 else if(key=='d'&&focus%columns<columns-1)next=focus+1;
 return next>=0&&next<count?next:focus;
}
inline bool directionKey(char key){return key=='w'||key=='a'||key=='s'||key=='d';}
}
