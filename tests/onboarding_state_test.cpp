#include "core/onboarding_state.h"

#include <QtTest>

class OnboardingStateTest final : public QObject
{
    Q_OBJECT
private slots:
    void doesNotInferActivationFromDiscovery();
    void reportsCandidateWhenNotReachable();
    void requiresExplicitActivationEvidence();
    void blocksActivationWithoutAdapter();
    void prioritizesAuthenticationAfterActivationCheck();
    void requiresVerifiedNetworkConfigurationForReady();
    void reportsReadyOnlyAfterVerification();
    void reportsInitializationInProgress();
    void reportsInitializedBeforeNetworkVerification();
    void reportsConfigurationRequiredAndConfiguring();
    void reportsFailureWithoutClaimingReady();
};

void OnboardingStateTest::doesNotInferActivationFromDiscovery()
{
    OnboardingEvidence evidence;
    evidence.observedOnNetwork = true;
    evidence.networkReachable = true;
    const auto result = OnboardingState::evaluate(evidence);
    QCOMPARE(result.state, OnboardingState::State::Reachable);
    QVERIFY(!result.label.contains(QStringLiteral("initial"), Qt::CaseInsensitive));
}

void OnboardingStateTest::reportsCandidateWhenNotReachable()
{
    OnboardingEvidence evidence;
    evidence.observedOnNetwork = true;
    const auto result = OnboardingState::evaluate(evidence);
    QCOMPARE(result.state, OnboardingState::State::Candidate);
    QVERIFY(!result.nextAction.isEmpty());
}

void OnboardingStateTest::requiresExplicitActivationEvidence()
{
    OnboardingEvidence evidence;
    evidence.observedOnNetwork = true;
    evidence.networkReachable = true;
    evidence.identityVerified = true;
    evidence.supportedAdapterAvailable = true;
    const auto result = OnboardingState::evaluate(evidence);
    QCOMPARE(result.state, OnboardingState::State::VerificationPending);

    evidence.activationStatusKnown = true;
    evidence.activationRequired = true;
    QCOMPARE(OnboardingState::evaluate(evidence).state,
             OnboardingState::State::InitializationRequired);
}

void OnboardingStateTest::blocksActivationWithoutAdapter()
{
    OnboardingEvidence evidence;
    evidence.observedOnNetwork = true;
    evidence.networkReachable = true;
    evidence.identityVerified = true;
    evidence.activationStatusKnown = true;
    evidence.activationRequired = true;
    QCOMPARE(OnboardingState::evaluate(evidence).state, OnboardingState::State::Unsupported);
}

void OnboardingStateTest::prioritizesAuthenticationAfterActivationCheck()
{
    OnboardingEvidence evidence;
    evidence.observedOnNetwork = true;
    evidence.networkReachable = true;
    evidence.identityVerified = true;
    evidence.authenticationRequired = true;
    evidence.supportedAdapterAvailable = true;
    QCOMPARE(OnboardingState::evaluate(evidence).state,
             OnboardingState::State::AuthenticationRequired);
}

void OnboardingStateTest::requiresVerifiedNetworkConfigurationForReady()
{
    OnboardingEvidence evidence;
    evidence.observedOnNetwork = true;
    evidence.networkReachable = true;
    evidence.identityVerified = true;
    evidence.supportedAdapterAvailable = true;
    QCOMPARE(OnboardingState::evaluate(evidence).state,
             OnboardingState::State::VerificationPending);
}

void OnboardingStateTest::reportsReadyOnlyAfterVerification()
{
    OnboardingEvidence evidence;
    evidence.observedOnNetwork = true;
    evidence.networkReachable = true;
    evidence.identityVerified = true;
    evidence.supportedAdapterAvailable = true;
    evidence.activationStatusKnown = true;
    evidence.activationVerified = true;
    evidence.networkConfigurationVerified = true;
    QCOMPARE(OnboardingState::evaluate(evidence).state, OnboardingState::State::Ready);
}

void OnboardingStateTest::reportsInitializationInProgress()
{
    OnboardingEvidence evidence;
    evidence.observedOnNetwork = true;
    evidence.networkReachable = true;
    evidence.identityVerified = true;
    evidence.activationInProgress = true;
    evidence.supportedAdapterAvailable = true;
    QCOMPARE(OnboardingState::evaluate(evidence).state,
             OnboardingState::State::InitializationInProgress);
}

void OnboardingStateTest::reportsInitializedBeforeNetworkVerification()
{
    OnboardingEvidence evidence;
    evidence.observedOnNetwork = true;
    evidence.networkReachable = true;
    evidence.identityVerified = true;
    evidence.activationStatusKnown = true;
    evidence.activationVerified = true;
    evidence.supportedAdapterAvailable = true;
    QCOMPARE(OnboardingState::evaluate(evidence).state, OnboardingState::State::Initialized);
}

void OnboardingStateTest::reportsConfigurationRequiredAndConfiguring()
{
    OnboardingEvidence evidence;
    evidence.observedOnNetwork = true;
    evidence.networkReachable = true;
    evidence.identityVerified = true;
    evidence.activationStatusKnown = true;
    evidence.activationVerified = true;
    evidence.supportedAdapterAvailable = true;
    evidence.configurationRequired = true;
    QCOMPARE(OnboardingState::evaluate(evidence).state,
             OnboardingState::State::ConfigurationRequired);

    evidence.configurationInProgress = true;
    QCOMPARE(OnboardingState::evaluate(evidence).state, OnboardingState::State::Configuring);
}

void OnboardingStateTest::reportsFailureWithoutClaimingReady()
{
    OnboardingEvidence evidence;
    evidence.observedOnNetwork = true;
    evidence.networkReachable = true;
    evidence.identityVerified = true;
    evidence.activationStatusKnown = true;
    evidence.activationVerified = true;
    evidence.networkConfigurationVerified = true;
    evidence.supportedAdapterAvailable = true;
    evidence.operationFailed = true;
    QCOMPARE(OnboardingState::evaluate(evidence).state, OnboardingState::State::Failed);
}

QTEST_APPLESS_MAIN(OnboardingStateTest)
#include "onboarding_state_test.moc"
