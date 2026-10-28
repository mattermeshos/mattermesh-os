#pragma once
// Field `index` (0-based) of a tab-separated row; "" when the row has fewer fields. Works on Arduino String.
namespace mm {
template<class S> inline S tsvField(const S &row,int index){int a=0;while(index--){a=row.indexOf('\t',a);if(a<0)return S();a++;}int b=row.indexOf('\t',a);return b<0?row.substring(a):row.substring(a,b);}
}
