#ifndef _timerex_h
#define _timerex_h

#include <circle/timer.h>
#include <circle/interrupt.h>
#include <circle/multicore.h>
#include <circle/synchronize.h>
#include <assert.h>

class CTimerEx : public CTimer		/// Extended system timer
{
public:
	/// \param pInterruptSystem Pointer to the interrupt system
	CTimerEx (CInterruptSystem *pInterruptSystem)
	:	CTimer (pInterruptSystem),
		m_hTimer (0)
	{
	}

	/// \brief Delay method, which powers down the core while waiting
	/// \param nMilliSeconds Delay in milliseconds (>= 20)
	/// \note Does work on core 0 only!
	void MsDelayPowerDown (unsigned nMilliSeconds)
	{
#ifdef ARM_ALLOW_MULTI_CORE
		assert (CMultiCoreSupport::ThisCore () == 0);
#endif

		unsigned nHZ = MSEC2HZ (nMilliSeconds);

		// Because of the granularity of the system timer,
		// smaller delays may result in a zero delay.
		assert (nHZ > 1);

		assert (!m_hTimer);
		m_hTimer = StartKernelTimer (nHZ, TimerHandler, nullptr, this);

		do
		{
			WaitForInterrupt ();
		}
		while (m_hTimer);

		assert (!m_hTimer);
	}

private:
	static void TimerHandler (TKernelTimerHandle hTimer, void *pParam, void *pContext)
	{
		CTimerEx *pThis = static_cast<CTimerEx *> (pContext);

		assert (hTimer == pThis->m_hTimer);
		pThis->m_hTimer = 0;
	}

private:
	volatile unsigned m_hTimer;
};

#endif
