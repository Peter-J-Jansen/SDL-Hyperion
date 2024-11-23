/* DAT.H        (C) Copyright Roger Bowler, 1999-2012                */
/*              (C) and others 2013-2024                             */
/*              Dynamic Address Translation                          */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

/* Interpretive Execution - (C) Copyright Jan Jaeger, 1999-2012      */
/* z/Architecture support - (C) Copyright Jan Jaeger, 1999-2012      */

/*-------------------------------------------------------------------*/
/*   ARCH_DEP section: compiled multiple times, ONCE for each arch.  */
/*-------------------------------------------------------------------*/

#if (ARCH_370_IDX == ARCH_IDX && !defined( DID_370_DAT_H )) \
 || (ARCH_390_IDX == ARCH_IDX && !defined( DID_390_DAT_H )) \
 || (ARCH_900_IDX == ARCH_IDX && !defined( DID_900_DAT_H ))

/*********************************************************************/
/*********************************************************************/
/**                                                                 **/
/**                LARGE NON-INLINED FUNCTIONS                      **/
/**                                                                 **/
/*********************************************************************/
/*********************************************************************/

void ARCH_DEP( purge_tlbe      )( REGS* regs, RADR pfra );
void ARCH_DEP( invalidate_tlb  )( REGS* regs, BYTE  mask );
void ARCH_DEP( invalidate_pte  )( BYTE ibyte, RADR pto, VADR vaddr, REGS* regs, bool local );

#if defined( FEATURE_ACCESS_REGISTERS )
U16  ARCH_DEP( translate_alet )( U32 alet, U16 eax, int acctype, REGS* regs, U32* asteo, U32 aste[] );
#endif

#if defined( FEATURE_DUAL_ADDRESS_SPACE )
U16 ARCH_DEP( translate_asn )( U16 asn, REGS* regs, U32* asteo, U32 aste[] );
#endif

/*-------------------------------------------------------------------*/
/*  All 3 build architecture variants of the below function must     */
/*  all be declared at once (we cannot wait for them to be declared  */
/*  later on a subsequent pass when the next build architecture is   */
/*  eventually built) since some functions might need invoke the     */
/*  "SIE_TRANSLATE_ADDR" macro, which might itself need to call      */
/*  the "translate_addr" function for a build architecture different */
/*  from the one currently being built (or currently executing).     */
/*-------------------------------------------------------------------*/
#ifndef TRANSLATE_ADDR_DEFINED
#define TRANSLATE_ADDR_DEFINED
  int s370_translate_addr ( U32 vaddr, int arn, REGS* regs, int acctype );
  int s390_translate_addr ( U32 vaddr, int arn, REGS* regs, int acctype );
  int z900_translate_addr ( U64 vaddr, int arn, REGS* regs, int acctype );
#endif

/*********************************************************************/
/*********************************************************************/
/**                                                                 **/
/**              SMALL INLINED FUNCTIONS FOR SPEED                  **/
/**                                                                 **/
/*********************************************************************/
/*********************************************************************/


/*-------------------------------------------------------------------*/
/* Purge all translation lookaside buffers for all CPUs              */
/*-------------------------------------------------------------------*/
inline void ARCH_DEP( purge_tlb_all )( REGS* regs, U16 cpuad )
{
    int  cpu;

    if (0xFFFF == cpuad && !IS_INTLOCK_HELD( regs ))  // (sanity check)
        CRASH();                                      // (logic error!)

    for (cpu=0; cpu < sysblk.maxcpu; cpu++)
    {
        if (1
            && IS_CPU_ONLINE(cpu)
            && (sysblk.regs[ cpu ]->cpubit & sysblk.started_mask)
            && (0
                || 0xFFFF == cpuad
                || sysblk.regs[ cpu ]->cpuad == cpuad
               )
        )
        {
            switch (sysblk.regs[ cpu ]->arch_mode)
            {
            case ARCH_370_IDX: s370_purge_tlb( sysblk.regs[ cpu ]); break;
            case ARCH_390_IDX: s390_purge_tlb( sysblk.regs[ cpu ]); break;
            case ARCH_900_IDX: z900_purge_tlb( sysblk.regs[ cpu ]); break;
            default: CRASH();
            }
        }
    }
}


/*-------------------------------------------------------------------*/
/* Purge specific translation lookaside buffer entry from all CPUs   */
/*-------------------------------------------------------------------*/
inline void ARCH_DEP( purge_tlbe_all )( REGS* regs, RADR pfra, U16 cpuad )
{
    int  cpu;

    if (0xFFFF == cpuad && !IS_INTLOCK_HELD( regs ))  // (sanity check)
        CRASH();                                      // (logic error!)

    for (cpu=0; cpu < sysblk.maxcpu; cpu++)
    {
        if (1
            && IS_CPU_ONLINE(cpu)
            && (sysblk.regs[ cpu ]->cpubit & sysblk.started_mask)
            && (0
                || 0xFFFF == cpuad
                || sysblk.regs[ cpu ]->cpuad == cpuad
               )
        )
        {
            switch (sysblk.regs[ cpu ]->arch_mode)
            {
            case ARCH_370_IDX: s370_purge_tlbe( sysblk.regs[ cpu ], pfra ); break;
            case ARCH_390_IDX: s390_purge_tlbe( sysblk.regs[ cpu ], pfra ); break;
            case ARCH_900_IDX: z900_purge_tlbe( sysblk.regs[ cpu ], pfra ); break;
            default: CRASH();
            }
        }
    }
}


#if defined( FEATURE_ACCESS_REGISTERS )
/*-------------------------------------------------------------------*/
/* Purge the ART lookaside buffer for all CPUs                       */
/*-------------------------------------------------------------------*/
inline void ARCH_DEP( purge_alb_all )( REGS* regs )
{
    int  cpu;

    if (!IS_INTLOCK_HELD( regs ))   // (sanity check)
        CRASH();                    // (logic error!)

    for (cpu=0; cpu < sysblk.maxcpu; cpu++)
    {
        if (1
            && IS_CPU_ONLINE(cpu)
            && (sysblk.regs[ cpu ]->cpubit & sysblk.started_mask)
        )
        {
            switch (sysblk.regs[ cpu ]->arch_mode)
            {
            case ARCH_390_IDX: s390_purge_alb( sysblk.regs[ cpu ]); break;
            case ARCH_900_IDX: z900_purge_alb( sysblk.regs[ cpu ]); break;
            default: CRASH(); // (370 doesn't have access registers)
            }
        }
    }
}
#endif /* defined( FEATURE_ACCESS_REGISTERS ) */


#if defined( FEATURE_DUAL_ADDRESS_SPACE )
/*-------------------------------------------------------------------*/
/* Perform ASN authorization process                                 */
/*                                                                   */
/* Input:                                                            */
/*      ax      Authorization index                                  */
/*      aste    Pointer to 16-word area containing a copy of the     */
/*              ASN second table entry associated with the ASN       */
/*      atemask Specifies which authority bit to test in the ATE:    */
/*              ATE_PRIMARY (for PT instruction)                     */
/*              ATE_SECONDARY (for PR, SSAR, and LASP instructions,  */
/*                             and all access register translations) */
/*      regs    Pointer to the CPU register context                  */
/*                                                                   */
/* Operation:                                                        */
/*      The AX is used to select an entry in the authority table     */
/*      pointed to by the ASTE, and an authorization bit in the ATE  */
/*      is tested.  For ATE_PRIMARY (X'80'), the P bit is tested.    */
/*      For ATE_SECONDARY (X'40'), the S bit is tested.              */
/*      Authorization is successful if the ATE falls within the      */
/*      authority table limit and the tested bit value is 1.         */
/*                                                                   */
/* Returns:                                                          */
/*      true  == Authorization *WAS* successful.                     */
/*      false == Authorization was *NOT* successful.                 */
/*                                                                   */
/*      A program check may be generated for addressing exception    */
/*      if the authority table entry address is invalid, and in      */
/*      this case the function does not return.                      */
/*-------------------------------------------------------------------*/
inline bool ARCH_DEP( authorize_asn )( U16 ax, U32 aste[], int atemask, REGS* regs )
{
RADR    ato;                            /* Authority table origin    */
int     atl;                            /* Authority table length    */
BYTE    ate;                            /* Authority table entry     */

    /* [3.10.3.1] Authority table lookup */

    /* Isolate the authority table origin and length */
    ato = aste[0] & ASTE0_ATO;
    atl = aste[1] & ASTE1_ATL;

    /* Authorization fails if AX is outside table */
    if ((ax & 0xFFF0) > atl)
        return true;

    /* Calculate the address of the byte in the authority
       table which contains the 2 bit entry for this AX */
    ato += (ax >> 2);

    /* Ignore carry into bit position 0 */
    ato &= 0x7FFFFFFF;

    /* Addressing exception if ATE is outside main storage */
    if (ato > regs->mainlim)
        goto auth_addr_excp;

    /* Load the byte containing the authority table entry
       and shift the entry into the leftmost 2 bits */
    ato = APPLY_PREFIXING( ato, regs->PX );

    /* Translate SIE host virt to SIE host abs. Note: macro
       is treated as a no-operation if SIE_MODE not active */
    SIE_TRANSLATE( &ato, ACCTYPE_SIE, regs );

    ate = regs->mainstor[ato];
    ate <<= ((ax & 0x03)*2);

    /* Set the main storage reference bit */
    ARCH_DEP( or_storage_key )( ato, STORKEY_REF );

    /* Authorization fails if the specified bit (either X'80' or
       X'40' of the 2 bit authority table entry) is zero */
    if ((ate & atemask) == 0)
        return true;

    /* Exit with successful return code */
    return false;

/* Conditions which always cause program check */
auth_addr_excp:
    regs->program_interrupt( regs, PGM_ADDRESSING_EXCEPTION );
    UNREACHABLE_CODE( return false );
}
#endif /* defined( FEATURE_DUAL_ADDRESS_SPACE ) */


/*-------------------------------------------------------------------*/
/*                           maddr_l                                 */
/*                PRIMARY DAT TLB LOOKUP FUNCTION                    */
/*-------------------------------------------------------------------*/
/*          For compatibility this function is usually               */
/*          invoked using the MADDRL macro in feature.h              */
/*-------------------------------------------------------------------*/
/*                                                                   */
/*  Convert logical address to absolute address. This is the DAT     */
/*  logic that does an accelerated TLB lookup to return the prev-    */
/*  iously determined value from an earlier translation for this     */
/*  logical address.  It performs a series of checks to ensure the   */
/*  values that were used in the previous translation (the results   */
/*  of which are in the corresponding TLB entry) haven't changed     */
/*  for the current address being translated.  If any of the cond-   */
/*  itions have changed (i.e. if any of the comparisons fail) then   */
/*  the TLB cannot be used (TLB miss) and "logical_to_main_l" is     */
/*  called to perform a full address translation. Otherwise if all   */
/*  of the conditions are still true (nothing has changed from the   */
/*  the last time we translated this address), then the previously   */
/*  translated address from the TLB is returned instead (TLB hit).   */
/*                                                                   */
/*  PLEASE NOTE that the address that is retrieved from the TLB is   */
/*  an absolute address from the Hercules guest's point of view but  */
/*  the address RETURNED TO THE CALLER is a Hercules host address    */
/*  pointing to MAINSTOR that Hercules can then directly use.        */
/*                                                                   */
/*  Input:                                                           */
/*                                                                   */
/*       addr    Logical address to be translated                    */
/*       len     Length of data access for PER SA purpose            */
/*       arn     Access register number or the special value:        */
/*                  USE_INST_SPACE                                   */
/*                  USE_REAL_ADDR                                    */
/*                  USE_PRIMARY_SPACE                                */
/*                  USE_SECONDARY_SPACE                              */
/*                  USE_HOME_SPACE                                   */
/*                  USE_ARMODE + access register number              */
/*               An access register number ORed with the special     */
/*               value USE_ARMODE forces this routine to use AR-mode */
/*               address translation regardless of the PSW address-  */
/*               space control setting.                              */
/*       regs    Pointer to the CPU register context                 */
/*       acctype Type of access requested: READ, WRITE, INSTFETCH,   */
/*               LRA, IVSK, TPROT, STACK, PTE, LPTEA                 */
/*       akey    Bits 0-3=access key, 4-7=zeroes                     */
/*                                                                   */
/*  Returns:                                                         */
/*                                                                   */
/*     If successful, a directly usable guest absolute storage       */
/*     MAINADDR address.                                             */
/*                                                                   */
/*     Otherwise if the logical address (as a result of having       */
/*     to call logical_to_main_l due to a TLB miss) causes an        */
/*     addressing, protection, or translation exception then a       */
/*     program check is generated and the function does not return.  */
/*-------------------------------------------------------------------*/
inline BYTE* ARCH_DEP( maddr_l )
    ( VADR addr, size_t len, const int arn, REGS* regs, const int acctype, const BYTE akey )
{
    /* Note: ALL of the below conditions must be true for a TLB hit
       to occur.  If ANY of them are false, then it's a TLB miss,
       requiring us to then perform a full DAT address translation.

       Note too that on the grand scheme of things the order/sequence
       of the below tests (if statements) is completely unimportant
       since ALL conditions must be checked anyway in order for a hit
       to occur, and it doesn't matter that a miss tests a few extra
       conditions since it's going to do a full translation anyway!
       (which is many, many instructions)
    */

    int  aea_crn  = (arn >= USE_ARMODE) ? 0 : regs->AEA_AR( arn );
    U16  tlbix    = TLBIX( addr );
    BYTE *maddr   = NULL;

    /* Non-zero AEA Control Register number? */
    if (aea_crn)
    {
        /* Same Addess Space Designator as before? */
        /* Or if not, is address in a common segment? */
        if (0
            || (regs->CR( aea_crn ) == regs->tlb.TLB_ASD( tlbix ))
            || (regs->AEA_COMMON( aea_crn ) & regs->tlb.common[ tlbix ])
        )
        {
            /* Storage Key zero? */
            /* Or if not, same Storage Key as before? */
            if (0
                || akey == 0
                || akey == regs->tlb.skey[ tlbix ]
            )
            {
                /* Does the page address match the one in the TLB? */
                /* (does a TLB entry exist for this page address?) */
                if (
                    ((addr & TLBID_PAGEMASK) | regs->tlbID)
                    ==
                    regs->tlb.TLB_VADDR( tlbix )
                )
                {
                    /* Is storage being accessed same way as before? */
                    if (acctype & regs->tlb.acc[ tlbix ])
                    {
                        /*------------------------------------------*/
                        /* TLB hit: use previously translated value */
                        /*------------------------------------------*/

                        if (acctype & ACC_CHECK)
                            regs->dat.storkey = regs->tlb.storkey[ tlbix ];

                        maddr = MAINADDR( regs->tlb.main[tlbix], addr );
                    }
                }
            }
        }
    }

    /*---------------------------------------*/
    /* TLB miss: do full address translation */
    /*---------------------------------------*/
    if (!maddr)
        maddr = ARCH_DEP( logical_to_main_l )( addr, arn, regs, acctype, akey, len );

#if defined( FEATURE_073_TRANSACT_EXEC_FACILITY )
    if (FACILITY_ENABLED( 073_TRANSACT_EXEC, regs ))
    {

  #if defined( TXF_BACKOUT_METHOD )

        U16   txf_backout_cache_lines_count_old;
        U16   txf_backout_cache_lines_count_locked;
        U16   txf_backout_cache_lines_count_new;
        U16   txf_cache_line_status_old;
        U16   txf_cache_line_status_initial;            
        BYTE *maddr_next;
        int   i;

        // First we handle the case of transactional accesses.
        /*  SA22-7832-12 Principles of Operation, page 5-99:
            SA22-7832-13 Principles of Operation, page 5-100:

            "Storage accesses for instruction and DAT- and ART-
                table fetches follow the non-transactional rules."
        */
        if ( 1
            && arn != USE_INST_SPACE    /* Not instruction fetching */
            && arn != USE_REAL_ADDR     /* Not a DAT- or ART access */
            && regs                     /* Only CPU access possible */             
            && regs->txf_tnd            /* transactional access     */      
            && !regs->txf_NTSTG )       /* not a NTSTG instruction  */ 
        { 
  #if !defined( TXF_COMMIT_METHOD )
            /* Check if our transaction has already been aborted, e.g. txf_abort_all( ) */
            
            if (regs->txf_tac)
            {
                PTT_TXF( "*TXF mad TAC", regs->txf_tac, regs->txf_contran, regs->txf_tnd );
                if (!(regs->txf_why & TXF_WHY_DELAYED_ABORT))
                {
                    regs->txf_why  |=  TXF_WHY_DELAYED_ABORT;
                    regs->txf_who   =  regs->cpuad;
                    regs->txf_loc   =  TRIMLOC( PTT_LOC );
                }
                ABORT_TRANS( regs, ABORT_RETRY_CC, regs->txf_tac );
                UNREACHABLE_CODE( return maddr );
            }
  #endif                    

#define HHC17740 "TXF %d transcpus, maddr=0x%16.16"PRIX64", len=%lu, %s page %lu cache lines access attempts" 
#if 0 /* PJJ : suppress HHC17740S's */     
            // Transactional accesses very seldom span more than one cache line, contraint ones    
            // were never observed with more than one; the much less frequent non-constrained  
            // ones were observed with a maximum of just 2 cache lines.  But the PoP allows
            // more than one.  The message below can be enabled to observe this.         
            if ( ( len != 1) && ( TXF_CACHE_LINES_COUNT( maddr, len ) != 1 ) )                      
            {  
#define                HHC17740 "TXF %d transcpus, maddr=0x%16.16"PRIX64", len=%lu, %s page %lu cache lines access attempts"                                                                                                                                                       
                WRMSG( HHC17740, "S", sysblk.txf_transcpus, (U64) maddr, len, "same" , TXF_CACHE_LINES_COUNT( maddr, len ) );  
            }
#endif /* PJJ : suppress HHC17740S's */              

            // Correct processing of transactional accesses requires the following actions sequence:
            // 1. Acquire the TXF_CACHE_LINE_LOCK ensuring our transaction's exclusive use delaying
            //    other possible users and obtain the txf_backout_cache_line_count at the same time.
            //
            //    If our lock acquisition fails, the lock is already taken, so a backout abort operation 
            //    is already in progress waiting for a delayed abort, which we will need to act upon with
            //    txf_backout_abort_cache_lines(... _IMMEDIATE), like for transactional access conflicts.  
            txf_backout_cache_lines_count_old = 
                regs->txf_backout_cache_lines_count & ( ~TXF_CACHE_LINE_LOCK );
            txf_backout_cache_lines_count_new = txf_backout_cache_lines_count_old;
            txf_backout_cache_lines_count_locked =
                ( txf_backout_cache_lines_count_old + 1 ) | TXF_CACHE_LINE_LOCK; 
            if ( !cmpxchg2( &txf_backout_cache_lines_count_old,
                             txf_backout_cache_lines_count_locked, 
                      &regs->txf_backout_cache_lines_count ) )
            {  

                // For each of the cache lines being accessed in this transaction ...        
                for ( i = 0, maddr_next = maddr; 
                      i < (int) TXF_CACHE_LINES_COUNT( maddr, len ); 
                      i++,   maddr_next += ZCACHE_LINE_SIZE )
                {

                    // 2. Updating our cache line's status, i.e. fetched or stored. 
                    //    We can only update the cache line's status if it's not in use 
                    //    yet, or only in use by the transaction of our own CPU. 
                    if ( TXF_ACCTYPE( acctype ) & ACC_WRITE )
                        txf_cache_line_status_initial = ( TXF_CACHE_LINE_STORED | regs->cpuad );
                    else
                        txf_cache_line_status_initial = ( TXF_CACHE_LINE_FETCHED | regs->cpuad );
                    txf_cache_line_status_old = TXF_CACHE_LINE_NOT_USED;   
                    if ( 0 
                        || !cmpxchg2( &txf_cache_line_status_old,
                                       txf_cache_line_status_initial,
                                      &TXF_CACHE_LINE_STATUS( maddr_next ) ) 
                        || ( txf_cache_line_status_old & TXF_CACHE_LINE_CPU_MASK ) == regs->cpuad )                                    
                    {
                        if ( txf_cache_line_status_old == TXF_CACHE_LINE_NOT_USED )
                        {    

                            //    OK, the cache line was not in use yet, so :
                            // 3. Saving the cache line for possible future backout in txf_backout_cache_line[ ].backout_cache_line
                            // 4. Storing the cache line maddr_next in txf_backout_cache_line[ ].maddr                    
                            //    Saving the cache line for possible future backout purposes includes NULLing the next .maddr 
                            //    and incrementing the txf_backout_cache_lines_count_new from the old one.
                            memcpy( regs->txf_backout_cache_lines[ txf_backout_cache_lines_count_new ].backout_cache_line,    
                                (BYTE *) ( (U64) maddr_next & ZCACHE_LINE_ADDRMASK ), ZCACHE_LINE_SIZE );
                            regs->txf_backout_cache_lines[   txf_backout_cache_lines_count_new ].maddr = 
                                (BYTE *) ( (U64) maddr_next & ZCACHE_LINE_ADDRMASK );   
                            regs->txf_backout_cache_lines[ ++txf_backout_cache_lines_count_new ].maddr = NULL;
                            if ( sysblk.txf_cache_line_maddr_lo )
                            { 
                                sysblk.txf_cache_line_maddr_lo = MIN( sysblk.txf_cache_line_maddr_lo, maddr_next );  
                                sysblk.txf_cache_line_maddr_hi = MAX( sysblk.txf_cache_line_maddr_hi, maddr_next ); 
                            }
                            else
                            { 
                                sysblk.txf_cache_line_maddr_lo = maddr_next;  
                                sysblk.txf_cache_line_maddr_hi = maddr_next; 
                            } 

                            // When we'd have to backout a transactionally stored cache line, we'd have no chance
                            // to perform a correct STORKEY backout, so we ensure it isn't needed, which it actually
                            // never is, so this code can be safely disabled; we've never seen the message.
                            BYTE storkey_bits = ARCH_DEP( get_storage_key )( (U64) ( ( maddr_next ) - sysblk.mainstor ) )
                                               & ( STORKEY_REF | STORKEY_CHANGE );
                            if ( storkey_bits != ( STORKEY_REF | STORKEY_CHANGE ) )
                            {
#define                                HHC17750 "TXF %d transcpus, maddr=0x%16.16"PRIX64", storkey_bits=%02X"                                                                                                                                                       
                                WRMSG( HHC17750, "W", sysblk.txf_transcpus, (U64) maddr, storkey_bits );
                                ARCH_DEP( or_storage_key )( (U64) ( ( maddr_next ) - sysblk.mainstor ),
                                                 ( STORKEY_REF | STORKEY_CHANGE ) );    
                            }      

                            //    We  check there is sill room for additional txf_backout_cache_lines. 
                            if ( txf_backout_cache_lines_count_new >= TXF_BACKOUT_CACHE_LINES_MAX )
                            {  
                                if ( TXF_ACCTYPE( acctype ) & ACC_WRITE )
                                    regs->txf_backout_tac = TAC_STORE_OVF;
                                else
                                    regs->txf_backout_tac = TAC_FETCH_OVF;                             
                            }
                        }        
                        else
                        {

                            // 5. A cache line in use by our own CPU's transaction may need to update the stored status.
                            if ( TXF_ACCTYPE( acctype ) & ACC_WRITE )
                                TXF_CACHE_LINE_STATUS( maddr_next ) = txf_cache_line_status_initial;     
                        }
                    }
            
                    // When the cache line was already in use in another transaction then we need to abort ours.

                    // SA22-7832-12 z/Architecture - Principles of Operation, pg 5-100 (top right),
                    // states "A fetch-conflict / store-conflict condition is detected when another
                    // CPU or the channel subsystem attempts to store / access a location that has been
                    // transactionally fetched / stored by this CPU".  The transaction by this CPU then
                    // needs to be aborted.  This implies that the first transaction should be aborted.
                    // However, in the case of two CPU's executing a transaction, it is not predicatable
                    // which of the conflicting ones started first, so we take the liberty to abort the
                    // the second starter, and not the first one.  We also believe this lowers the
                    // number of transaction retries.                    
                    else
                    {  
                        if ( TXF_ACCTYPE( acctype ) & ACC_WRITE )
                            regs->txf_backout_tac = TAC_STORE_CNF;
                        else
                            regs->txf_backout_tac = TAC_FETCH_CNF;                                                                         
                    } 
                }                 

                // 6. All of our cache line updates completed we need to unlock the transaction's cache line,
                //    and at the same time replace the txf_backout_cache_lines_count_old with the new one,
                //    noting that we already incremented and locked the count to avoid a zero-count locked state
                //    as this is allowed exclusively to mark a cache line as backed out awaiting delayed abort.   
// tested OK // i = 3 ;
// tested OK // while ( i-- && 
                while ( cmpxchg2( &txf_backout_cache_lines_count_locked,
                                   txf_backout_cache_lines_count_new, 
                            &regs->txf_backout_cache_lines_count ) )
                {
#define                    HHC17743 "TXF: %s%02X: %s%s internal logic error : cache line exclusive update violated"
                    WRMSG( HHC17743, "S", TXF_CPUAD( regs ), TXF_QSIE( regs ), TXF_CONSTRAINED( regs->txf_contran ) );
                } 
            } 
            // The transaction's cache line now being unlocked, we are done, except if a backout abort code
            // has been set, in which case we have to backout the transaction store accesses and abort. 
            if ( regs->txf_backout_tac )
            { 
                regs->txf_who = regs->cpuad; 
                regs->txf_why |= TXF_WHY_CONFLICT;                 
                regs->txf_loc = TRIMLOC( PTT_LOC );                     
                txf_backout_abort_cache_lines( regs, TXF_BACKOUT_ABORT_IMMEDIATE);
            }          
        } /* regs->txf_tnd != 0 */

        // Non-transactional access is only allowed for non-conflict cases.  Otherwise
        // the cache line needs to be backed out first (and the transaction aborted,
        // which will be a delayed abort), prior to allowing the access to take place.
        else if ( sysblk.txf_cache_line_maddr_lo && ( len > 0 ) )
        {
            if ( ( len == 1)  || ( TXF_CACHE_LINES_COUNT( maddr, len ) == 1 ) )                      
                maddr_next = maddr;
            else 
            { 
                maddr_next = MAX( maddr, sysblk.txf_cache_line_maddr_lo ); 

#if 0 /* PJJ : suppress HHC17740I's */
                if ( ( len != 1) && ( TXF_CACHE_LINES_COUNT( maddr, len ) > 1 ) )                      
                {  
                    if (   ( maddr           <= sysblk.txf_cache_line_maddr_hi )    
                        && ( maddr + len - 1 >= sysblk.txf_cache_line_maddr_lo ) ) 
                    {          
                        if ( ( ( ( (U64) (maddr) & 0x0FFF ) + (len) - 1 ) >> 12 ) > 0 ) /* 4K page boundary crossed */  
                        {                                                                                                                                                               
                            WRMSG( HHC17740, "I", sysblk.txf_transcpus, (U64) maddr, len, "multiple", TXF_CACHE_LINES_COUNT( maddr, len ) );  
                        }
                        else if ( ( len != 1) && ( TXF_CACHE_LINES_COUNT( maddr, len ) > 2 ) )                      
                        {                                                                                                                                           
                            WRMSG( HHC17740, "I", sysblk.txf_transcpus, (U64) maddr, len, "same", TXF_CACHE_LINES_COUNT( maddr, len ) );  
                        }
                    }      
                } 
#endif /* PJJ : suppress HHC17740I's */                
                
            }                    
            do      
            {      
                // A conflict exists when accessing an already stored cache line,
                // or when writing into an already fetched cache line.
                if (0    
                    || ( TXF_CACHE_LINE_IS_STORED(  maddr_next ) )
                    || ( TXF_CACHE_LINE_IS_FETCHED( maddr_next ) && ( TXF_ACCTYPE( acctype ) & ACC_WRITE ) ) ) 
                {
                    REGS *tregs = TXF_CACHE_LINE_REGS( maddr_next );
                    if ( TXF_CACHE_LINE_IS_STORED(  maddr_next ) )
                        tregs->txf_backout_tac = TAC_STORE_CNF;
                    else
                        tregs->txf_backout_tac = TAC_FETCH_CNF;
                    tregs->txf_who  = regs->cpuad; 
                    tregs->txf_why |= TXF_WHY_DELAYED_ABORT | TXF_WHY_CONFLICT;  
                    tregs->txf_loc  = TRIMLOC( PTT_LOC );                      
                    txf_backout_abort_cache_lines( tregs, TXF_BACKOUT_ABORT_DELAYED ); /* NTRANS CNF */                    
                } 
                maddr_next += ZCACHE_LINE_SIZE;    
            }
            while ( maddr_next <= MIN( maddr + len - 1, sysblk.txf_cache_line_maddr_hi ) );      
        } 

  #endif /* defined( TXF_BACKOUT_METHOD ) */

  #if defined( TXF_COMMIT_METHOD )

        /* SA22-7832-12 Principles of Operation, page 5-99:
           SA22-7832-13 Principles of Operation, page 5-100:

             "Storage accesses for instruction and DAT- and ART-
              table fetches follow the non-transactional rules."
        */
        if (0
            || !regs
            || !regs->txf_tnd
            || arn == USE_INST_SPACE    /* Instruction fetching */
            || arn == USE_REAL_ADDR     /* Address translation  */
        )
            return maddr;

        /* Quick exit if NTSTG call */
        if (regs->txf_NTSTG)
        {
            regs->txf_NTSTG = false;
            return maddr;
        }

        /* Translate to alternate TXF address */
        maddr = TXF_MADDRL( addr, len, arn, regs, acctype, maddr );

  #endif /* defined( TXF_COMMIT_METHOD ) */

    }
#endif

    return maddr;
}


/*-------------------------------------------------------------------*/
/*  We only need to compile this header ONCE for each architecture!  */
/*-------------------------------------------------------------------*/

#if      ARCH_370_IDX == ARCH_IDX
  #define DID_370_DAT_H
#endif

#if      ARCH_390_IDX == ARCH_IDX
  #define DID_390_DAT_H
#endif

#if      ARCH_900_IDX == ARCH_IDX
  #define DID_900_DAT_H
#endif

#endif // #if (ARCH_xxx_IDX == ARCH_IDX && !defined( DID_xxx_DAT_H )) ...

/* end of DAT.H */
