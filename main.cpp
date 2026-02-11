#include "PCH.h"
#include "Ini/Ini.h"
#include <filesystem>
#include <functional>
#include "ActorsManager/ActorsManager.h"

static auto getIni() -> const ini::map&;

static auto GetManager() -> ActorsManager& {
	static auto manager = ActorsManager("Data\\F4SE\\Plugins\\SAKR.json");
	return manager;
}

void MessageHandler(F4SE::MessagingInterface::Message* a_msg);
void initAfterDllWasLoaded();
void initAfterGameWasStarted();
void initAfterGameDataWasLoaded();

extern "C" DLLEXPORT auto F4SEAPI F4SEPlugin_Query(const F4SE::QueryInterface* a_f4se, F4SE::PluginInfo* a_info) -> bool 
{
#ifndef NDEBUG
	auto sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
#else
	auto path = logger::log_directory();
	if (!path) {
		return false;
	}

	*path /= fmt::format(FMT_STRING("{}.log"), Version::Name);
	auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
#endif

	auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));

	log->set_level(spdlog::level::info);
	log->flush_on(spdlog::level::info);

	spdlog::set_default_logger(std::move(log));
	//spdlog::set_pattern("%g(%#): [%^%l%$] %v"s);
	spdlog::set_pattern("[%m/%d/%Y - %T] [%^%l%$] %v"s);

	logger::info("{} v{}", Version::Name, Version::Version);

	a_info->infoVersion = F4SE::PluginInfo::kVersion;
	a_info->name = Version::Name;
	a_info->version = Version::VersionCount;

	if (a_f4se->IsEditor()) {
		logger::critical("loaded in editor");
		return false;
	}

	const auto ver = a_f4se->RuntimeVersion();
	if (ver < F4SE::RUNTIME_1_10_162) {
		logger::critical("unsupported runtime v{}", ver.string());
		return false;
	}

	return true;
}

extern "C" DLLEXPORT auto F4SEAPI F4SEPlugin_Load(const F4SE::LoadInterface* a_f4se) -> bool
{
	F4SE::Init(a_f4se);

	/*const auto serialization = F4SE::GetSerializationInterface();
	if (!serialization) {
		logger::critical("Failed to get F4SE serialization interface, marking as incompatible.");
		return false;
	}
	else {
		serialization->SetUniqueID(Version::UID);
		logger::critical("Registered with F4SE serialization interface.");
	}*/

	const auto messaging = F4SE::GetMessagingInterface();
	if (!messaging || !messaging->RegisterListener(MessageHandler)) {
		logger::critical("Failed to get F4SE messaging interface, marking as incompatible.");
		return false;
	}
	else {
		logger::critical("Registered with F4SE messaging interface.");
		logger::critical("Starting...");
	}

	initAfterDllWasLoaded();

	return true;
}

auto MessageHandler(F4SE::MessagingInterface::Message* a_msg) -> void
{
	if (!a_msg) {
		return;
	}

	switch (a_msg->type) {
	case F4SE::MessagingInterface::kPreLoadGame: {
		break;
	}
	case F4SE::MessagingInterface::kGameDataReady:
		initAfterGameDataWasLoaded();
		break;
	case F4SE::MessagingInterface::kPostLoadGame:
		initAfterGameWasStarted();
		break;
	case F4SE::MessagingInterface::kNewGame:
		break;
	}

}

auto initAfterGameWasStarted() -> void {
}


auto initAfterGameDataWasLoaded() -> void {
	GetManager();
}

static auto getIni() -> const ini::map& {
	namespace fs = std::filesystem;

	static const fs::path iniPath = []() -> fs::path {
		fs::path p = fs::current_path() / "Data" / "F4SE" / "Plugins" / "SAKR.ini";

		try {
			if (!fs::exists(p)) {
				fs::create_directories(p.parent_path());
				std::ofstream ofs(p);
				if (ofs) {
					logger::info("Created default config file at {}", p.string());
				}
				else {
					logger::warn("Failed to create default config file at {} (ofstream failed)", p.string());
				}
			}
		}
		catch (const std::exception& e) {
			logger::error("Exception while ensuring ini at {}: {}", p.string(), e.what());
		}

		return p;
		}();

	static ini::map instance{ iniPath };
	return instance;
}

auto initAfterDllWasLoaded() -> void {

}