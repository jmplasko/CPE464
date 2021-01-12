#ifndef __PDU_H__
#define __PDU_H__

uint8_t* createPDU(uint32_t sequenceNumber, uint8_t flag, uint8_t* payload, int dataLen);
void outputPDU(uint8_t* aPDU, int pduLength);

#endif
