/*
 * Copyright (C) 2005 by egnite Software GmbH. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * For additional information see http://www.ethernut.de/
 *
 */

/*
 * $Log$
 * Revision 1.6  2009/01/30 08:55:23  haraldkipp
 * Enable IRQ clocks.
 *
 * Revision 1.5  2008/08/11 06:59:09  haraldkipp
 * BSD types replaced by stdint types (feature request #1282721).
 *
 * Revision 1.4  2008/07/26 09:43:01  haraldkipp
 * Added support for retrieving and setting the interrupt mode.
 *
 * Revision 1.3  2006/06/28 17:10:15  haraldkipp
 * Include more general header file for ARM.
 *
 * Revision 1.2  2006/04/07 12:19:51  haraldkipp
 * Fast interrupts are now enabled correctly.
 * Mode set/get ioctl added and priority set/get removed.
 * Interrupt entry will no longer push R8-R12 on stack.
 *
 * Revision 1.1  2005/10/24 08:56:09  haraldkipp
 * First check in.
 *
 */

#include <arch/arm.h>
#include <dev/irqreg.h>

#ifndef NUT_IRQPRI_FIQ
#define NUT_IRQPRI_FIQ  4
#endif

static int FastIrqCtl(int cmd, void *param);

IRQ_HANDLER sig_FIQ = {
#ifdef NUT_PERFMON
    0,              /* Interrupt counter, ir_count. */
#endif
    NULL,           /* Passed argument, ir_arg. */
    NULL,           /* Handler subroutine, ir_handler. */
    FastIrqCtl      /* Interrupt control, ir_ctl. */
};

/*!
 * \brief Fast interrupt entry.
 *
 * \todo Use different entry/exit sequence for the fast interrupt.
 */
static void FastIrqEntry(void) NUT_NAKED_FUNC;
void FastIrqEntry(void)
{
    FIQ_ENTRY();
#ifdef NUT_PERFMON
    sig_FIQ.ir_count++;
#endif
    if (sig_FIQ.ir_handler) {
        (sig_FIQ.ir_handler) (sig_FIQ.ir_arg);
    }
    FIQ_EXIT();
}

/*!
 * \brief Fast interrupt control.
 *
 * \param cmd   Control command.
 *              - NUT_IRQCTL_INIT Initialize and disable interrupt.
 *              - NUT_IRQCTL_STATUS Query interrupt status.
 *              - NUT_IRQCTL_ENABLE Enable interrupt.
 *              - NUT_IRQCTL_DISABLE Disable interrupt.
 *              - NUT_IRQCTL_GETMODE Query interrupt mode.
 *              - NUT_IRQCTL_SETMODE Set interrupt mode.
 *              - NUT_IRQCTL_GETCOUNT Query and clear interrupt counter.
 * \param param Pointer to optional parameter.
 *
 * \return 0 on success, -1 otherwise.
 */
static int FastIrqCtl(int cmd, void *param)
{
    int rc = 0;
    unsigned int *ival = (unsigned int *)param;
    int_fast8_t enabled = inr(AIC_IMR) & _BV(FIQ_ID);

    /* Disable interrupt. */
    if (enabled) {
        outr(AIC_IDCR, _BV(FIQ_ID));
    }

    switch(cmd) {
    case NUT_IRQCTL_INIT:
        /* Set the vector. */
        outr(AIC_SVR(FIQ_ID), (unsigned int)FastIrqEntry);
        /* Initialize to edge triggered with defined priority. */
        outr(AIC_SMR(FIQ_ID), AIC_SRCTYPE_EXT_NEGATIVE_EDGE | NUT_IRQPRI_FIQ);
        /* Clear interrupt */
        outr(AIC_ICCR, _BV(FIQ_ID));
        break;
    case NUT_IRQCTL_STATUS:
        if (enabled) {
            *ival |= 1;
        }
        else {
            *ival &= ~1;
        }
        break;
    case NUT_IRQCTL_ENABLE:
        enabled = 1;
        break;
    case NUT_IRQCTL_DISABLE:
        enabled = 0;
        break;
    case NUT_IRQCTL_GETMODE:
        {
            unsigned int val = inr(AIC_SMR(FIQ_ID)) & AIC_SRCTYPE;
            if (val == AIC_SRCTYPE_EXT_LOW_LEVEL) {
                *ival = NUT_IRQMODE_LOWLEVEL;
            } else if (val == AIC_SRCTYPE_EXT_HIGH_LEVEL) {
                *ival = NUT_IRQMODE_HIGHLEVEL;
            } else if (val == AIC_SRCTYPE_EXT_POSITIVE_EDGE) {
                *ival = NUT_IRQMODE_RISINGEDGE;
            } else  {
                *ival = NUT_IRQMODE_FALLINGEDGE;
            }
        }
        break;
    case NUT_IRQCTL_SETMODE:
        if (*ival == NUT_IRQMODE_LOWLEVEL) {
            outr(AIC_SMR(FIQ_ID), (inr(AIC_SMR(FIQ_ID)) & ~AIC_SRCTYPE) | AIC_SRCTYPE_EXT_LOW_LEVEL);
        } else if (*ival == NUT_IRQMODE_HIGHLEVEL) {
            outr(AIC_SMR(FIQ_ID), (inr(AIC_SMR(FIQ_ID)) & ~AIC_SRCTYPE) | AIC_SRCTYPE_EXT_HIGH_LEVEL);
        } else if (*ival == NUT_IRQMODE_FALLINGEDGE) {
            outr(AIC_SMR(FIQ_ID), (inr(AIC_SMR(FIQ_ID)) & ~AIC_SRCTYPE) | AIC_SRCTYPE_EXT_NEGATIVE_EDGE);
        } else  if (*ival == NUT_IRQMODE_RISINGEDGE) {
            outr(AIC_SMR(FIQ_ID), (inr(AIC_SMR(FIQ_ID)) & ~AIC_SRCTYPE) | AIC_SRCTYPE_EXT_POSITIVE_EDGE);
        } else  {
            rc = -1;
        }
        break;
#ifdef NUT_PERFMON
    case NUT_IRQCTL_GETCOUNT:
        *ival = (unsigned int)sig_FIQ.ir_count;
        sig_FIQ.ir_count = 0;
        break;
#endif
    default:
        rc = -1;
        break;
    }

    /* Enable interrupt. */
    if (enabled) {
        outr(AIC_IECR, _BV(FIQ_ID));
#if defined(PMC_PCER)
        outr(PMC_PCER, _BV(FIQ_ID));
#endif
    }
    return rc;
}
