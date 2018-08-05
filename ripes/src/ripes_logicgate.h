#ifndef LOGICGATE_H
#define LOGICGATE_H

#include "ripes_primitive.h"

namespace ripes {

enum class LogicGateType { AND, OR, XOR, NOT, NAND, NOR, XNOR };

template <uint32_t inputCount, uint32_t width>
class LogicGate : public Component<width> {
public:
    LogicGate(LogicGateType t) {
        static_assert(inputCount > 0, "Input count must be greater than 0");

        switch (t) {
            case LogicGateType::AND: {
                m_f = [=] {
                    auto out = this->m_inputs[0];
                    for (int i = 1; i < this->m_inputs.size(); i++) {
                        out &= this->m_inputs[i];
                    }
                    return out;
                };
                break;
            }
            case LogicGateType::OR: {
                m_f = [=] {
                    auto out = this->m_inputs[0];
                    for (int i = 1; i < this->m_inputs.size(); i++) {
                        out |= this->m_inputs[i];
                    }
                    return out;
                };
                break;
            }
            case LogicGateType::XOR: {
                throw std::runtime_error("implement me!");
                break;
            }
            case LogicGateType::NOT: {
                m_f = [=] { return ~this->m_inputs[0]; };
                break;
            }
            case LogicGateType::NAND: {
                throw std::runtime_error("implement me!");
                break;
            }
            case LogicGateType::NOR: {
                throw std::runtime_error("implement me!");
                break;
            }
            case LogicGateType::XNOR: {
                throw std::runtime_error("implement me!");
                break;
            }
        }
    }

    void propagate() override { propagateComponent(m_f); }

    void verifySubtype() const override {
        // Nothing to verify
    }

    std::function<std::array<bool, width>()> m_f;
};
}  // namespace ripes

#endif  // LOGICGATE_H
