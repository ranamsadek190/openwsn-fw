#include "opendefs.h"
#include "sfx.h"
#include "neighbors.h"
#include "sixtop.h"
#include "scheduler.h"
#include "schedule.h"
#include "openqueue.h"
#include "idmanager.h"
#include "icmpv6rpl.h"
#include "scheduler.h"

//=========================== definition ======================================

//#define SFX_DEBUG

// the threshold here is a relative ratio to CELL_USAGE_CALCULATION_WINDOWS (schedule.c)
// those value must be less than CELL_USAGE_CALCULATION_WINDOWS. If the cell usage is 
// less than 
//          SFX_DELETE_THRESHOLD/CELL_USAGE_CALCULATION_WINDOWS
// remove a slot. If cell usage is more than 
//          SFX_ADD_THRESHOLD/CELL_USAGE_CALCULATION_WINDOWS
// add a lost. Else, nothing happens.
#define SFX_ADD_THRESHOLD          8    // 8 means 80% cell usage
#define SFX_TARGET                 6    
#define SFX_DELETE_THRESHOLD       4

//=========================== variables =======================================

sfx_vars_t sfx_vars;

//=========================== prototypes ======================================

void sfx_addCell_task(void);
void sfx_removeCell_task(void);
void sfx_cellUsageCalculation_task(void);

//=========================== public ==========================================

void sfx_init(void) {
    sfx_vars.backoff = 0;
}

void sfx_notif_addedCell(void) {
   scheduler_push_task(sfx_addCell_task,TASKPRIO_SFX);
}

void sfx_notif_removedCell(void) {
   scheduler_push_task(sfx_removeCell_task,TASKPRIO_SFX);
}

//=========================== private =========================================

void sfx_addCell_task(void) {
   open_addr_t          neighbor;
   bool                 foundNeighbor;
   
   // get preferred parent
   foundNeighbor = icmpv6rpl_getPreferredParentEui64(&neighbor);
   if (foundNeighbor==FALSE) {
      return;
   }
   
   if (sixtop_setHandler(SIX_HANDLER_SFX)==FALSE){
      // one sixtop transcation is happening, only one instance at one time
      return;
   }
   
   // call sixtop
   sixtop_request(
      IANA_6TOP_CMD_ADD,
      &neighbor,
      1
   );
}

void sfx_removeCell_task(void) {
   open_addr_t          neighbor;
   bool                 foundNeighbor;
   
   // get preferred parent
   foundNeighbor = icmpv6rpl_getPreferredParentEui64(&neighbor);
   if (foundNeighbor==FALSE) {
      return;
   }
   
   if (sixtop_setHandler(SIX_HANDLER_SFX)==FALSE){
      // one sixtop transcation is happening only one instance at one time
      return;
   }
   // call sixtop
   sixtop_request(
      IANA_6TOP_CMD_DELETE,
      &neighbor,
      1
   );
}

void sfx_notifyNewSlotframe(void){
    scheduler_push_task(sfx_cellUsageCalculation_task,TASKPRIO_SFX);
}

void sfx_setBackoff(uint8_t value){
    sfx_vars.backoff = value;
}

// ========================== private =========================================
void sfx_cellUsageCalculation_task(){
   open_addr_t          neighbor; 
   bool                 foundNeighbor;
   uint16_t             numberOfCells;
   uint16_t             cellUsage;
   
   if (idmanager_getIsDAGroot()){
      return;
   }
   
   if (sfx_vars.backoff>0){
      sfx_vars.backoff -= 1;
      return;
   }
   
   // get preferred parent
   foundNeighbor = icmpv6rpl_getPreferredParentEui64(&neighbor);
   if (foundNeighbor==FALSE) {
      return;
   }
   
   numberOfCells  = schedule_getCellsCounts(
            SCHEDULE_MINIMAL_6TISCH_DEFAULT_SLOTFRAME_HANDLE,
            CELLTYPE_TX,
            &neighbor);
//   numberOfCells += schedule_getCellsCounts(
//            SCHEDULE_MINIMAL_6TISCH_DEFAULT_SLOTFRAME_HANDLE,
//            CELLTYPE_TXRX,
//            NULL);
   cellUsage  = schedule_getTotalCellUsageStatus(CELLTYPE_TX,&neighbor);
//   cellUsage += schedule_getTotalCellUsageStatus(CELLTYPE_TXRX,NULL);
   
   if(numberOfCells==0){
       if (sixtop_setHandler(SIX_HANDLER_SFX)==FALSE){
          // one sixtop transcation is happening, only one instance at one time
          return;
       }
       // call sixtop
       sixtop_request(
          IANA_6TOP_CMD_ADD,
          &neighbor,
          1
       );
       return;
   }
   
   // cell usage scheduling, bandwith estimation algorithm
   if (cellUsage/numberOfCells>=SFX_ADD_THRESHOLD){
       if (sixtop_setHandler(SIX_HANDLER_SFX)==FALSE){
          // one sixtop transcation is happening, only one instance at one time
          return;
       }
       // call sixtop
       sixtop_request(
          IANA_6TOP_CMD_ADD,
          &neighbor,
          cellUsage/SFX_TARGET-numberOfCells+1
       );
   } else {
     if (cellUsage/numberOfCells<SFX_DELETE_THRESHOLD){
         // at least keep one cell
         if (numberOfCells>1){
             if (sixtop_setHandler(SIX_HANDLER_SFX)==FALSE){
                // one sixtop transcation is happening, only one instance at one time
                return;
             }
             // call sixtop
             sixtop_request(
                IANA_6TOP_CMD_DELETE,
                &neighbor,
                1
             );
         } else {
           // do not delete shared cell
         }
     } else {
        // nothing happens
     }
   }
}