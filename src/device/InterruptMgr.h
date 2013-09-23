//
// Copyright 2013 (C). Alex Robenko. All rights reserved.
//

#pragma once

#include <cstddef>
#include <type_traits>
#include <array>
#include <algorithm>

#include "embxx/util/Assert.h"
#include "embxx/util/StaticFunction.h"

namespace device
{

namespace interrupt
{

inline
void enable()
{
    __asm volatile("cpsie i");
}

inline
void disable()
{
    __asm volatile("cpsid i");
}

}  // namespace interrupt

template <typename THandler = embxx::util::StaticFunction<void (), 16> >
class InterruptMgr
{
public:
    typedef THandler HandlerFunc;
    enum IrqId {
        IrqId_Timer,
        IrqId_NumOfIds // Must be last
    };

    InterruptMgr();

    template <typename TFunc>
    void registerHandler(IrqId id, TFunc&& handler);

    void enableInterrupt(IrqId id);

    void disableInterrupt(IrqId id);

    void handleInterrupt();

private:


    typedef std::uint32_t EntryType;

    struct IrqInfo {
        IrqInfo();

        HandlerFunc handler_;
        EntryType mask_;
        volatile EntryType* pendingPtr_;
        volatile EntryType* enablePtr_;
        volatile EntryType* disablePtr_;
    };

    typedef std::array<IrqInfo, IrqId_NumOfIds> IrqsArray;

    IrqsArray irqs_;

    static constexpr volatile EntryType* IrqBasicPending =
        reinterpret_cast<EntryType*>(0x2000B200);

    static constexpr volatile EntryType* IrqPending1 =
        reinterpret_cast<EntryType*>(0x2000B204);

    static constexpr volatile EntryType* IrqPending2 =
        reinterpret_cast<EntryType*>(0x2000B208);

    static constexpr volatile EntryType* IrqEnableBasic =
        reinterpret_cast<EntryType*>(0x2000B218);

    static constexpr volatile EntryType* IrqEnable1 =
        reinterpret_cast<EntryType*>(0x2000B210);

    static constexpr volatile EntryType* IrqEnable2 =
        reinterpret_cast<EntryType*>(0x2000B214);

    static constexpr volatile EntryType* IrqDisableBasic =
        reinterpret_cast<EntryType*>(0x2000B224);

    static constexpr volatile EntryType* IrqDisable1 =
        reinterpret_cast<EntryType*>(0x2000B21C);

    static constexpr volatile EntryType* IrqDisable2 =
        reinterpret_cast<EntryType*>(0x2000B220);

    static const EntryType MaskPending1 = 0x00000100;
    static const EntryType MaskPending2 = 0x00000200;

};

// Implementation

template <typename THandler>
InterruptMgr<THandler>::InterruptMgr()
{
    auto& timerIrq = irqs_[IrqId_Timer];
    timerIrq.mask_ = 0x00000001;
    timerIrq.pendingPtr_ = IrqBasicPending;
    timerIrq.enablePtr_ = IrqEnableBasic;
    timerIrq.disablePtr_ = IrqDisableBasic;
}

template <typename THandler>
template <typename TFunc>
void InterruptMgr<THandler>::registerHandler(
    IrqId id,
    TFunc&& handler)
{
    GASSERT(id < IrqId_NumOfIds);
    irqs_[id].handler_ = std::forward<TFunc>(handler);
}

template <typename THandler>
void InterruptMgr<THandler>::enableInterrupt(IrqId id)
{
    GASSERT(id < IrqId_NumOfIds);
    auto& info = irqs_[id];
    *info.enablePtr_ = info.mask_;
}

template <typename THandler>
void InterruptMgr<THandler>::disableInterrupt(IrqId id)
{
    GASSERT(id < IrqId_NumOfIds);
    auto& info = irqs_[id];
    *info.disablePtr_ = info.mask_;
}

template <typename THandler>
void InterruptMgr<THandler>::handleInterrupt()
{
    auto irqsBasic = *IrqBasicPending;
    auto irqsPending1 = *IrqPending1;
    auto irqsPending2 = *IrqPending2;

    std::for_each(irqs_.begin(), irqs_.end(),
        [this, irqsBasic, irqsPending1, irqsPending2](IrqInfo& info)
        {
            bool invoke = false;
            do {
                if (info.pendingPtr_ == IrqBasicPending) {
                    if ((info.mask_ & irqsBasic) != 0) {
                        invoke = true;
                    }
                    break;
                }

                if (info.pendingPtr_ == IrqPending1) {
                    if ((irqsPending1 & MaskPending1) != 0) {
                        invoke = true;
                    }
                    break;
                }

                if (info.pendingPtr_ == IrqPending2) {
                    if ((irqsPending2 & MaskPending2) == 0) {
                        invoke = true;
                    }
                    break;
                }
            } while (false);

            if (invoke) {
                if (info.handler_) {
                    info.handler_();
                }
            }
        });
}

template <typename THandler>
InterruptMgr<THandler>::IrqInfo::IrqInfo()
    : mask_(0),
      pendingPtr_(0),
      enablePtr_(0),
      disablePtr_(0)
{
}

}  // namespace device


