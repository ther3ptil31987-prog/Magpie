#include "RtssResourceTestFixture.h"

static unsigned checks = 0;
static void Require(bool value, const char* message) {
    ++checks;
    if (!value) throw std::runtime_error(message);
}
static bool Contains(const hstring& text, const hstring& part) {
    return std::wstring_view(text).find(std::wstring_view(part)) != std::wstring_view::npos;
}
int main() {
    try {
        init_apartment(apartment_type::multi_threaded);
        for (auto language : {L"en-US", L"zh-Hans", L"zh-Hant"}) {
            Resources::Core::ResourceContext::SetGlobalQualifierValue(L"Language", language);
            const auto resources = ResourceLoader::GetForCurrentView(CommonSharedConstants::APP_RESOURCE_MAP_ID);
            for (auto key : ResourceKeys) Require(!resources.GetString(key).empty(), "Empty RTSS resource");
            ProfileViewModel vm;
            for (auto state : States) {
                vm.status.state = state;
                Require(!vm.RtssStatusTitle().empty(), "Status title uses native map");
                (void)vm.RtssStatusDescription();
            }
            vm.status = {}; vm.status.state = RtssState::Written;
            vm.status.rateKnown = true; vm.status.currentRate = 59.94; vm.status.scope = RtssScope::Global;
            Require(vm.RtssStatusTitle() == resources.GetString(L"Rtss_Status_Written"), "Ready title");
            const auto expectedRate = hstring(fmt::format(fmt::runtime(std::wstring_view(resources.GetString(L"Rtss_ProfileRate"))), 59.94));
            Require(Contains(vm.RtssStatusDescription(), expectedRate), "Localized configured rate");
            Require(Contains(vm.RtssStatusDescription(), resources.GetString(L"Rtss_Detail_Global")), "Localized actual scope");
            vm.status.state = RtssState::Failed; vm.status.profileSaved = true; vm.status.failureStage = L"Rtss_Failure_Reload";
            Require(vm.RtssStatusTitle() == resources.GetString(L"Rtss_Status_Failed"), "Failure title");
            Require(Contains(vm.RtssStatusDescription(), resources.GetString(L"Rtss_ProfileSaved")) &&
                Contains(vm.RtssStatusDescription(), resources.GetString(L"Rtss_Failure_Reload")), "Partial save and reload failure remain separate facts");
            Require(!Contains(vm.RtssStatusDescription(), resources.GetString(L"Rtss_ScalingContinues")), "Inactive scaling wording");
            auto& service = RtssService::Get();
            service.status = {}; service.status.state = RtssState::NotInstalled;
            Require(vm.RtssProgramStatus() == resources.GetString(L"Rtss_Inventory_Missing"), "Missing program inventory");
            service.status.rtss.executable = L"RTSS.exe"; service.status.rtss.running = true;
            Require(vm.RtssProgramStatus() == resources.GetString(L"Rtss_Inventory_Running"), "Running program inventory");
            service.initializationUnknown = true;
            Require(vm.RtssSelfStatus() == resources.GetString(L"Rtss_Inventory_Unknown"), "Unknown self inventory");
            service.initializationUnknown = false; service.initialized = true;
            Require(vm.RtssSelfStatus() == resources.GetString(L"Rtss_Inventory_Unlimited"), "Known self inventory");
            service.status.manualOperation = true; service.status.profile = L"MSI Afterburner"; service.status.state = RtssState::ToolStartFailed;
            Require(Contains(vm.RtssManagementResult(), resources.GetString(L"Rtss_Status_AfterburnerStartFailed")), "Optional tool failure belongs to management");
            service.status.state = RtssState::ToolStarted;
            Require(Contains(vm.RtssManagementResult(), resources.GetString(L"Rtss_Management_Started")), "Completed tool launch result");
            service.status = {}; service.status.selfInitialization = true; service.status.state = RtssState::Failed;
            service.status.profileSaved = true; service.status.failureStage = L"Rtss_Failure_Reload";
            Require(Contains(vm.RtssManagementResult(), resources.GetString(L"Rtss_ProfileSaved")) &&
                Contains(vm.RtssManagementResult(), resources.GetString(L"Rtss_Failure_Reload")), "Manual partial result remains factual");
        }
        std::cout << "RTSS native resource text: " << checks << " checks passed across 3 languages.\n";
        return 0;
    } catch (const hresult_error& e) {
        std::cerr << "HRESULT: 0x" << std::hex << uint32_t(e.code().value) << " " << to_string(e.message()) << '\n';
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; }
    return 1;
}
