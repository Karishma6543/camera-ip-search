#pragma once

#include <QString>

struct OnboardingEvidence
{
    bool observedOnNetwork{false};
    bool networkReachable{false};
    bool identityVerified{false};
    bool activationStatusKnown{false};
    bool activationRequired{false};
    bool authenticationRequired{false};
    bool supportedAdapterAvailable{false};
    bool networkConfigurationVerified{false};
};

class OnboardingState final
{
public:
    enum class State {
        NotObserved,
        Candidate,
        Reachable,
        Identified,
        InitializationRequired,
        AuthenticationRequired,
        Unsupported,
        VerificationPending,
        Ready
    };

    struct Result {
        State state{State::NotObserved};
        QString label;
        QString nextAction;
    };

    // Activation is reported as required only when explicit protocol evidence
    // says so; vendor names, open ports and missing credentials are not evidence.
    static Result evaluate(const OnboardingEvidence &evidence);
};
