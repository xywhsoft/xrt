/* Application checks for these offline examples; no public library API. */
#ifndef XOAUTH2_EXAMPLE_SUPPORT_H
#define XOAUTH2_EXAMPLE_SUPPORT_H
static bool oauthExampleSetString(xvalue* object, const char* key, const char* text)
{
    return text != NULL && xrtValueObjectSetNew(object, xrtStrView(key),
        xrtValueString(xrtStrView(text)));
}
static bool oauthExampleClaimString(const xvalue* object, const char* key,
                                   char* output, size_t capacity)
{
    xstrview value;
    if(output==NULL || capacity==0) return false;
    output[0]=0;
    if(!xrtValueGetString(xrtValueObjectGet(object,xrtStrView(key)),&value) ||
       value.Size==0 || value.Size>=capacity || memchr(value.Data,0,value.Size)!=NULL)
        return false;
    memcpy(output,value.Data,value.Size); output[value.Size]=0;
    return true;
}
#endif
