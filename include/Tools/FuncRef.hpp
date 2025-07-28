#ifndef hpp_FuncRef_hpp
#define hpp_FuncRef_hpp

// We need concepts too
#include "Concepts.hpp"
#include <new>
#include <type_traits>
#include <utility>

namespace Tools
{
    namespace Details
    {
        struct matching_function_pointer_tag {};
        struct matching_functor_tag          {};
        struct invalid_functor_tag           {};

        template <typename Func, typename Return, typename... Args>
        struct get_callable_tag
        {
            // use unary + to convert to function pointer
            template <typename T,
                    typename Result = decltype((+std::declval<T&>())(std::declval<Args>()...))>
            static matching_function_pointer_tag test(
                int, T& obj,
                typename std::enable_if<compatible_return_type<Result, Return>::value, int>::type = 0);

            template <typename T,
                    typename Result = decltype(std::declval<T&>()(std::declval<Args>()...))>
            static matching_functor_tag test(
                short, T& obj,
                typename std::enable_if<compatible_return_type<Result, Return>::value, int>::type = 0);

            static invalid_functor_tag test(...);

            using type = decltype(test(0, std::declval<Func&>()));
        };

        template<typename ... Ts>
        struct AlignedUnion {
            alignas(Ts...) std::byte storage[std::max({sizeof(Ts)...})];
            void* get() noexcept { return &storage; }
            const void* get() const noexcept { return &storage; }
        };
    } // namespace Details

    // Forward declare the function_ref object
    template <typename Signature> class function_ref;

    namespace Details
    {
        template<typename>
        struct function_ref_trait { using type = void; };

        template<typename Signature>
        struct function_ref_trait<function_ref<Signature>>
        {
            using type = function_ref<Signature>;
            using return_type = typename type::return_type;
        };
    } // namespace Details

    /** A reference to a function.

        This is a lightweight reference to a function.
        It can refer to any function that is compatible with given signature.

        A function is compatible if it is callable with regular function call syntax from the given
        argument types, and its return type is either implicitly convertible to the specified return
        type or the specified return type is `void`.

        In general it will store a pointer to the functor,
        requiring an lvalue.
        But if it is created with a function pointer or something convertible to a function pointer,
        it will store the function pointer itself.
        This allows creating it from stateless lambdas.
        @note Due to implementation reasons, it does not support member function pointers, as it requires regular function call syntax.
        Create a reference to the object returned by [std::mem_fn](), if that is required. */
    template <typename Return, typename... Args>
    class function_ref<Return(Args...)>
    {
    public:
        using return_type = Return;
        using signature = Return(Args...);

        /** Create a reference from a function pointer that's exactly matching the signature */
        function_ref(Return (*fptr)(Args...)) : function_ref(Details::matching_function_pointer_tag{}, fptr) {}

        /** Create a reference from a function pointer that can be converted to the expected signature */
        template <typename Return2, typename... Args2>
        function_ref(Return2 (*fptr)(Args2...))
        requires(Concepts::compatible_return_type_v<Return2, Return>)
        : function_ref(Details::matching_function_pointer_tag{}, fptr) {}

        /** Create a reference to a functor */
        /// \effects Creates a reference to the function created by the stateless lambda.
        /// \notes This constructor is intended for stateless lambdas,
        /// which are implicitly convertible to function pointers.
        /// It does not participate in overload resolution,
        /// unless the type is implicitly convertible to a function pointer
        /// that is compatible with the specified signature.
        /// \notes Due to to implementation reasons,
        /// it does not work for polymorphic lambdas,
        /// it needs an explicit cast to the desired function pointer type.
        /// A polymorphic lambda convertible to a direct match function pointer,
        /// works however.
        /// \param 1
        /// \exclude
        template <typename Functor>
        function_ref(const Functor& f)
        requires(std::is_same_v<typename Details::get_callable_tag<Functor, Return, Args...>::type, Details::matching_function_pointer_tag>)
        : function_ref(Details::matching_function_pointer_tag{}, +f) {}

        /** Create a reference to the specified functor */
        template <typename Functor>
        explicit function_ref(Functor& f)
        requires(std::is_same_v<typename Details::function_ref_trait<Functor>::type, void> and std::is_same_v<typename Details::get_callable_tag<Functor, Return, Args...>::type, Details::matching_functor_tag>)
        : cb_(&invoke_functor<Functor>)
        {
            // Ref to this functor
            ::new (storage_.get()) void*(&f);
        }

        /// Converting copy constructor.
        /// \effects Creates a reference to the same function referred by `other`.
        /// \notes This constructor does not participate in overload resolution,
        /// unless the signature of `other` is compatible with the specified signature.
        /// \notes This constructor may create a bigger conversion chain.
        /// For example, if `other` has signature `void(const char*)` it can refer to a function taking
        /// `std::string`. If this signature than accepts a type `T` implicitly convertible to `const
        /// char*`, calling this will call the function taking `std::string`, converting `T ->
        /// std::string`, even though such a conversion would be ill-formed otherwise. \param 1 \exclude
        template <typename Functor>
        explicit function_ref(Functor& f)
        requires(!std::is_same_v<typename Details::function_ref_trait<Functor>::type, void>
            and !std::is_same_v<typename Details::function_ref_trait<Functor>::type, function_ref>
            and std::is_same_v<typename Details::get_callable_tag<Functor, Return, Args...>::type, Details::matching_functor_tag>)
         : cb_(&invoke_functor<Functor>)
        {
            // Ref to this function_ref
            ::new (storage_.get()) void*(&f);
        }

        /** Rebinds the reference to the specified functor. */
        template <typename Functor>
        requires(!std::is_same_v<typename std::decay<Functor>::type, function_ref>)
        void assign(Functor&& f) noexcept
        {
            auto ref = function_ref(std::forward<Functor>(f));
            storage_ = ref.storage_;
            cb_      = ref.cb_;
        }

        /// \effects Invokes the stored function with the specified arguments and returns the result.
        Return operator()(Args... args) const
        {
            return cb_(storage_.get(), static_cast<Args>(args)...);
        }

    private:
        template <typename Functor>
        static Return invoke_functor(const void* memory, Args... args)
        {
            using ptr_t   = void*;
            ptr_t    ptr  = *static_cast<const ptr_t*>(memory);
            Functor& func = *static_cast<Functor*>(ptr);
            return static_cast<Return>(func(static_cast<Args>(args)...));
        }

        template <typename PointerT, typename StoredT>
        static Return invoke_function_pointer(const void* memory, Args... args)
        {
            auto ptr  = *static_cast<const StoredT*>(memory);
            auto func = reinterpret_cast<PointerT>(ptr);
            return static_cast<Return>(func(static_cast<Args>(args)...));
        }

        template <typename Return2, typename... Args2>
        function_ref(Details::matching_function_pointer_tag, Return2 (*fptr)(Args2...))
        {
            using pointer_type        = Return2 (*)(Args2...);
            using stored_pointer_type = void (*)();

            ::new (storage_.get()) stored_pointer_type(reinterpret_cast<stored_pointer_type>(fptr));

            cb_ = &invoke_function_pointer<pointer_type, stored_pointer_type>;
        }

        using storage  = Details::AlignedUnion<void*, Return (*)(Args...)>;
        using callback = Return (*)(const void*, Args...);

        storage  storage_;
        callback cb_;
    };
}

#endif