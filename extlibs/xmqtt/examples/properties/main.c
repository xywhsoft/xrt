#include <xmqtt.h>
#include <stdio.h>

int main(void)
{
	/* Offline property section only; no broker/network or owned result cleanup.
	 * Server PUBLISH may include repeated subscription identifiers. */
	xmqttproperty Input[3]={0}; Input[0].Id=XMQTT_PROPERTY_PAYLOAD_FORMAT; Input[0].Type=XMQTT_PROPERTY_BYTE; Input[0].Value.Number=1;
	Input[1].Id=XMQTT_PROPERTY_USER; Input[1].Type=XMQTT_PROPERTY_STRING_PAIR;
	Input[1].Value.Pair=(xmqttstringpair){XRT_STR_LITERAL("trace"),XRT_STR_LITERAL("abc")};
	Input[2].Id=XMQTT_PROPERTY_SUBSCRIPTION_ID; Input[2].Type=XMQTT_PROPERTY_VARIABLE; Input[2].Value.Number=7;
	unsigned char Wire[128]; size_t Required, Written;
	if (!xmqttPropertiesSize(Input,3,XMQTT_PROPERTIES_PUBLISH,XMQTT_SERVER_TO_CLIENT,NULL,&Required) ||
		!xmqttPropertiesWrite(Wire,sizeof(Wire),Input,3,XMQTT_PROPERTIES_PUBLISH,XMQTT_SERVER_TO_CLIENT,NULL,&Written) || Written!=Required) return 1;
	xmqttproperty Parsed[4]; xmqttpropertydocument Doc;
	if (xmqttPropertiesRead((xbytesview){Wire,Written},XMQTT_PROPERTIES_PUBLISH,XMQTT_SERVER_TO_CLIENT,NULL,Parsed,4,&Doc)!=XMQTT_PROPERTIES_DONE ||
		Doc.Count!=3 || Parsed[2].Value.Number!=7) return 2;
	printf("properties=%zu bytes=%zu subscription=%u\n",Doc.Count,Doc.Consumed,(unsigned)Parsed[2].Value.Number); return 0;
}
