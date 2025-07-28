#ifndef hpp_FuncRef_hpp
#define hpp_FuncRef_hpp

// We need concepts too
#include "Concepts.hpp"
#include <type_traits>
#include <functional>

namespace Tools
{
    template <typename Signature>
    struct function_ref;

    template <typename Return, typename ... Args>
    struct function_ref<Return(Args...)>
    {
        using Call = Return (*)(Args..., void*);

        template <typename Fun>
        static Return call(Args... args, void* object) {
            if constexpr(!std::is_pointer_v<Fun>) {
                using Pointer = std::add_pointer_t<Fun>;
                return static_cast<Return>(std::invoke(static_cast<Fun&&>(*static_cast<Pointer>(object)), static_cast<Args&&>(args)...));
            } else {
                return static_cast<Return>(std::invoke(reinterpret_cast<Fun>(object), static_cast<Args&&>(args)...));
            }
        }

        constexpr explicit operator bool() const noexcept { return obj; }
        Return operator()(Args... args) const {
            return caller ? caller(static_cast<Args&&>(args)..., obj) : Return{};
        }

        /** Build from callable object */
        template <typename Fun>
        requires(!std::is_same_v<function_ref, std::decay_t<Fun>> && std::is_invocable_r_v<Return, Fun&&, Args&&...>)
        constexpr function_ref(Fun && fun) noexcept {
            auto ptr = std::addressof(fun);
            obj = const_cast<void*>(static_cast<void const*>(ptr));
            caller = &function_ref::template call<Fun>;
        }

        /** Build from a function pointer. */
        template <typename Fun>
        requires(std::is_function_v<Fun> && std::is_invocable_r_v<Return, Fun&, Args&&...>)
        constexpr function_ref(Fun* fun) noexcept {
            if (fun) {
                obj = const_cast<void*>(reinterpret_cast<void const*>(fun));
                caller = &function_ref::template call<Fun*>;
            }
        }


    private:
        void * obj = nullptr;
        Call caller = nullptr;
    };    

}

#endif