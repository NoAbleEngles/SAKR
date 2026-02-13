#pragma once
#include "F4SE/F4SE.h"
#include "RE/Fallout.h"
#include <mutex>

namespace SAKR
{
	// Helper class to create a compatible function for BSTThreadScrapFunction
	class PapyrusArgsHelper
	{
	public:
		using ArgsArray = RE::BSScrapArray<RE::BSScript::Variable>;
		
		static bool SetArgs(ArgsArray& dest) {
			std::lock_guard<std::mutex> lock(s_mutex);
			if (s_currentArgs) {
				dest = *s_currentArgs;
				return true;
			}
			return false;
		}

		static void SetCurrentArgs(ArgsArray* args) {
			std::lock_guard<std::mutex> lock(s_mutex);
			s_currentArgs = args;
		}

	private:
		static inline ArgsArray* s_currentArgs = nullptr;
		static inline std::mutex s_mutex;
	};

	// Отправка событий в Papyrus через F4SE
	class PapyrusEvents
	{
	public:
		// Отправляет событие всем скриптам, зарегистрированным через RegisterForExternalEvent
		template <class... Args>
		static void SendEvent(const std::string& eventName, Args... args)
		{
			auto papyrus = F4SE::GetPapyrusInterface();
			if (!papyrus) {
				logger::error("Failed to get Papyrus interface for event: {}", eventName);
				return;
			}

			auto vm = RE::GameVM::GetSingleton()->GetVM().get();
			if (!vm) {
				logger::error("Failed to get VM for event: {}", eventName);
				return;
			}

			// Подготавливаем аргументы
			RE::BSScrapArray<RE::BSScript::Variable> arguments;
			PackArgs(arguments, vm, args...);

			// Получаем список зарегистрированных обработчиков
			struct EventContext {
				RE::BSScript::IVirtualMachine* vm;
				RE::BSScrapArray<RE::BSScript::Variable> args;
			};

			EventContext context{ vm, arguments };

			papyrus->GetExternalEventRegistrations(
				eventName,
				&context,
				[](uint64_t handle, const char* scriptName, const char* callbackName, void* dataPtr) {
					auto* ctx = static_cast<EventContext*>(dataPtr);
					
					// Set the current arguments for the helper
					PapyrusArgsHelper::SetCurrentArgs(&ctx->args);
					
					// Create function pointer reference
					auto funcPtr = &PapyrusArgsHelper::SetArgs;
					
					// Cast to the expected type - this is a hack but necessary given msvc::function limitations
					auto* bstFunc = reinterpret_cast<const RE::BSTThreadScrapFunction<bool(RE::BSScrapArray<RE::BSScript::Variable>&)>*>(&funcPtr);
					
					// Call DispatchMethodCall
					RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> nullCallback;
					ctx->vm->DispatchMethodCall(handle, scriptName, callbackName, *bstFunc, nullCallback);
					
					// Clear the current args
					PapyrusArgsHelper::SetCurrentArgs(nullptr);
				}
			);
		}

	private:
		static void PackArgs(RE::BSScrapArray<RE::BSScript::Variable>& args, RE::BSScript::IVirtualMachine* vm)
		{
			// Base case: no more arguments
		}

		template <class T, class... Rest>
		static void PackArgs(RE::BSScrapArray<RE::BSScript::Variable>& args, RE::BSScript::IVirtualMachine* vm, T first, Rest... rest)
		{
			RE::BSScript::Variable var;
			PackVariable(var, vm, first);
			args.push_back(var);
			PackArgs(args, vm, rest...);
		}

		static void PackVariable(RE::BSScript::Variable& var, RE::BSScript::IVirtualMachine* vm, std::int32_t value) { var = value; }
		static void PackVariable(RE::BSScript::Variable& var, RE::BSScript::IVirtualMachine* vm, std::uint32_t value) { var = value; }
		static void PackVariable(RE::BSScript::Variable& var, RE::BSScript::IVirtualMachine* vm, float value) { var = value; }
		static void PackVariable(RE::BSScript::Variable& var, RE::BSScript::IVirtualMachine* vm, bool value) { var = value; }
		static void PackVariable(RE::BSScript::Variable& var, RE::BSScript::IVirtualMachine* vm, const char* value) { var = RE::BSFixedString(value); }
		static void PackVariable(RE::BSScript::Variable& var, RE::BSScript::IVirtualMachine* vm, const RE::BSFixedString& value) { var = value; }
		
		template <typename T>
		static void PackVariable(RE::BSScript::Variable& var, RE::BSScript::IVirtualMachine* vm, T* form)
		{
			if (form) {
				auto& policy = vm->GetObjectHandlePolicy();
				var = static_cast<std::uint32_t>(policy.GetHandleForObject(RE::BSScript::GetVMTypeID<T>(), form));
			}
		}
	};
}
