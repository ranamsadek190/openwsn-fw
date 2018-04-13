#include "heli.h"
#include "msp430x26x.h"

//=========================== variables =======================================

//=========================== prototypes ======================================

//=========================== public ==========================================

void heli_init(void) {
   // set pins P1.2,3 as output
   P1OUT   &= ~0x0C;
   P1DIR   |=  0x0C;
}

void heli_on(void) {
   P1OUT   |=  0x0C;
}

void heli_off(void) {
   P1OUT   &= ~0x0C;
}

//=========================== private =========================================