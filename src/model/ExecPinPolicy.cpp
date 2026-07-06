// Category-driven exec pin normalization.

#include "ExecPinPolicy.h"

#include <utility>

bool CategoryHasExecFlow(const std::string& category)
{
    return category != "Pure" && category != "Object";
}

std::vector<PinDef> NormalizeClassExecPins(const std::string& category,
                                           std::vector<PinDef> pins)
{
    std::vector<PinDef> dataIn;
    std::vector<PinDef> dataOut;
    std::vector<PinDef> execOut;
    for (PinDef& pin : pins) {
        if (pin.type == PinType::Exec) {
            // Authored exec inputs are dropped: a flow category gets
            // exactly one automatic exec input.
            if (pin.direction == PinDirection::Output) {
                execOut.push_back(std::move(pin));
            }
        } else if (pin.direction == PinDirection::Input) {
            dataIn.push_back(std::move(pin));
        } else {
            dataOut.push_back(std::move(pin));
        }
    }

    const bool flow = CategoryHasExecFlow(category);
    std::vector<PinDef> result;

    if (flow && category != "Event") {
        PinDef execIn;
        execIn.direction = PinDirection::Input;
        execIn.type = PinType::Exec;
        result.push_back(std::move(execIn));
    }
    for (PinDef& pin : dataIn) {
        result.push_back(std::move(pin));
    }

    if (flow) {
        if (execOut.empty()) {
            PinDef then;
            then.direction = PinDirection::Output;
            then.type = PinType::Exec;
            then.name = "then";
            execOut.push_back(std::move(then));
        }
        for (PinDef& pin : execOut) {
            result.push_back(std::move(pin));
        }
    }
    for (PinDef& pin : dataOut) {
        result.push_back(std::move(pin));
    }
    return result;
}
