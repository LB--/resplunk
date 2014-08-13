#ifndef resplunk_event_Events_HeaderPlusPlus
#define resplunk_event_Events_HeaderPlusPlus

#include "resplunk/util/TemplateImplRepo.hpp"

#include <cstdint>
#include <limits>
#include <type_traits>
#include <functional>
#include <memory>
#include <map>
#include <stack>
#include <set>
#include <tuple>

namespace resplunk
{
	namespace event
	{
		struct Event;

		struct ListenerPriority final
		{
			using Priority_t = std::intmax_t;

			static constexpr Priority_t FIRST = std::numeric_limits<Priority_t>::min();
			static constexpr Priority_t LAST  = std::numeric_limits<Priority_t>::max();

			constexpr ListenerPriority() noexcept
			: priority{}
			{
			}
			constexpr ListenerPriority(Priority_t p) noexcept
			: priority{p}
			{
			}
			constexpr ListenerPriority(ListenerPriority const &) = default;
			ListenerPriority &operator=(ListenerPriority const &) = delete;
			constexpr ListenerPriority(ListenerPriority &&) = default;
			ListenerPriority &operator=(ListenerPriority &&) = delete;

			constexpr operator Priority_t() const noexcept
			{
				return priority;
			}

			friend constexpr bool operator==(ListenerPriority const &a, ListenerPriority const &b) noexcept
			{
				return a.priority == b.priority;
			}
			friend constexpr bool operator<(ListenerPriority const &a, ListenerPriority const &b) noexcept
			{
				return a.priority < b.priority;
			}

		private:
			Priority_t const priority;
		};

		template<typename EventT>
		struct Processor
		{
			static_assert(std::is_base_of<Event, EventT>::value, "EventT must derive from Event");
			using E = EventT;
			Processor(ListenerPriority priority = ListenerPriority{}) noexcept
			{
				listen(priority);
			}
			Processor(Processor const &) = delete;
			Processor &operator=(Processor const &) = delete;
			Processor(Processor &&) = delete;
			Processor &operator=(Processor &&) = delete;
			virtual ~Processor() noexcept
			{
				ignore();
			}

		protected:
			void listen(ListenerPriority priority = ListenerPriority{}) noexcept
			{
				return E::listen(*this, priority);
			}
			void ignore() noexcept
			{
				return E::ignore(*this);
			}

		private:
			virtual void onEvent(E &e) const noexcept = 0;

			friend typename E::Registrar_t;
		};

		template<typename EventT>
		struct LambdaProcessor
		: Processor<EventT>
		{
			using Lambda_t = std::function<void (EventT &e) /*const*/>;
			LambdaProcessor(Lambda_t l, ListenerPriority priority = ListenerPriority{}) noexcept
			: Processor<EventT>(priority)
			, lambda(l)
			{
			}

		private:
			Lambda_t lambda;
			virtual void onEvent(EventT &e) const noexcept override
			{
				return lambda(e);
			}
		};

		template<typename EventT>
		struct Reactor
		{
			static_assert(std::is_base_of<Event, EventT>::value, "EventT must derive from Event");
			using E = EventT;
			Reactor(ListenerPriority priority = ListenerPriority{}) noexcept
			{
				listen(priority);
			}
			Reactor(Reactor const &) = delete;
			Reactor &operator=(Reactor const &) = delete;
			Reactor(Reactor &&) = delete;
			Reactor &operator=(Reactor &&) = delete;
			virtual ~Reactor() noexcept
			{
				ignore();
			}

		protected:
			void listen(ListenerPriority priority = ListenerPriority{}) noexcept
			{
				return E::listen(*this, priority);
			}
			void ignore() noexcept
			{
				return E::ignore(*this);
			}

		private:
			virtual void onEvent(E const &e) noexcept = 0;

			friend typename E::Registrar_t;
		};

		template<typename EventT>
		struct LambdaReactor
		: Reactor<EventT>
		{
			using Lambda_t = std::function<void (EventT const &e)>;
			LambdaReactor(Lambda_t l, ListenerPriority priority = ListenerPriority{})
			: Reactor<EventT>(priority) noexcept
			, lambda(l)
			{
			}

		private:
			Lambda_t lambda;
			virtual void onEvent(EventT const &e) noexcept override
			{
				return lambda(e);
			}
		};

		template<typename EventT>
		struct EventRegistrar final
		{
			static_assert(std::is_base_of<Event, EventT>::value, "EventT must derive from Event");
			using E = EventT;
			using Processor = Processor<EventT>;
			using Reactor = Reactor<EventT>;
			EventRegistrar() = delete;

			static void listen(Processor const &p, ListenerPriority priority = ListenerPriority{}) noexcept
			{
				processors().emplace(priority, std::cref(p));
			}
			static void listen(Reactor &r, ListenerPriority priority = ListenerPriority{}) noexcept
			{
				reactors().emplace(priority, std::ref(r));
			}

			static void ignore(Processor const &p) noexcept
			{
				auto &procs = processors();
				for(auto it = procs.begin(); it != procs.end(); )
				{
					if(std::addressof(p) == std::addressof(it->second.get()))
					{
						it = procs.erase(it);
					}
					else ++it;
				}
			}
			static void ignore(Reactor &r) noexcept
			{
				auto &reacts = reactors();
				for(auto it = reacts.begin(); it != reacts.end(); )
				{
					if(std::addressof(r) == std::addressof(it->second.get()))
					{
						it = reacts.erase(it);
					}
					else ++it;
				}
			}

			static void process(E &e) noexcept
			{
				if(!e.shouldProcess())
				{
					return;
				}
				auto &procs = processors();
				for(auto it = procs.begin(); it != procs.end(); ++it)
				{
					it->second.get().onEvent(e);
				}
			}
			static void react(E const &e) noexcept
			{
				if(!e.shouldReact())
				{
					return;
				}
				auto &reacts = reactors();
				for(auto it = reacts.begin(); it != reacts.end(); ++it)
				{
					it->second.get().onEvent(e);
				}
			}

		private:
			struct Key final
			{
				Key() = delete;
			};
			using Processors_t = std::multimap<ListenerPriority, std::reference_wrapper<Processor const>>;
			using Reactors_t = std::multimap<ListenerPriority, std::reference_wrapper<Reactor>>;
			static Processors_t &processors() noexcept
			{
				return util::TemplateImplRepo::get<Key, Processors_t>();
			}
			static auto reactors() noexcept
			-> Reactors_t &
			{
				return util::TemplateImplRepo::get<Key, Reactors_t>();
			}
		};

		namespace impl
		{
			template<typename T, typename...>
			struct Unwrapper;
			template<typename T, typename First, typename... Rest>
			struct Unwrapper<T, First, Rest...> final
			{
				static_assert(std::is_base_of<Event, First>::value, "ParentT must derive from Event");
				using Next = Unwrapper<T, Rest...>;
				Unwrapper() = delete;
				static void process(T &t) noexcept
				{
					t.First::process();
					Next::process(t);
				}
				static void react(T const &t) noexcept
				{
					t.First::react();
					Next::react(t);
				}
				static auto parents(T &t) noexcept
				{
					return tuple_cat(std::tuple<First &>{t}, Next::parents(t));
				}
				static auto parents(T const &t) noexcept
				{
					return tuple_cat(std::tuple<First const &>{t}, Next::parents(t));
				}
			};
			template<typename T>
			struct Unwrapper<T> final
			{
				static_assert(std::is_base_of<Event, typename T::E>::value, "Only Event can be root");
				Unwrapper() = delete;
				static void process(T &t) noexcept
				{
				}
				static void react(T const &t) noexcept
				{
				}
				static auto parents(T &t) noexcept
				-> std::tuple<>
				{
					return {};
				}
				static auto parents(T const &t) noexcept
				-> std::tuple<>
				{
					return {};
				}
			};
			using Guard_t = std::stack<std::set<std::type_index>>;
			struct ProcessKey final
			{
				ProcessKey() = delete;
			};
			struct ReactKey final
			{
				ReactKey() = delete;
			};
			struct PR
			{
				virtual ~PR() noexcept = default;
				virtual void process() noexcept = 0;
				virtual void react() const noexcept = 0;
			};
		}
		template<typename EventT, typename... ParentT>
		struct Implementor
		: virtual impl::PR
		, virtual ParentT...
		{
			using Unwrapper_t = impl::Unwrapper<Implementor, ParentT...>;
			using E = EventT;
			using Parents_t = std::tuple<ParentT &...>;
			using ConstParents_t = std::tuple<ParentT const &...>;
			using Implementor_t = Implementor;
			using Processor_t = Processor<EventT>;
			using Reactor_t = Reactor<EventT>;
			using Registrar_t = EventRegistrar<EventT>;
			static constexpr bool MI = (sizeof...(ParentT) > 1);
			static constexpr bool ROOT = (sizeof...(ParentT) == 0);
			virtual ~Implementor() = 0;

			static void listen(Processor_t const &p, ListenerPriority priority = ListenerPriority{}) noexcept
			{
				return Registrar_t::listen(p, priority);
			}
			static void listen(Reactor_t &r, ListenerPriority priority = ListenerPriority{}) noexcept
			{
				return Registrar_t::listen(r, priority);
			}

			static void ignore(Processor_t const &p) noexcept
			{
				return Registrar_t::ignore(p);
			}
			static void ignore(Reactor_t &r) noexcept
			{
				return Registrar_t::ignore(r);
			}

			virtual void process() noexcept override
			{
				using namespace impl;
				Guard_t &guard = util::TemplateImplRepo::get<ProcessKey, Guard_t>();
				if(typeid(*this) == typeid(E))
				{
					guard.emplace();
					guard.top().emplace(typeid(E));
				}
				else if(guard.top().find(typeid(E)) != guard.top().end())
				{
					return;
				}
				else
				{
					guard.top().emplace(typeid(E));
				}

				Unwrapper_t::process(*this);
				Registrar_t::process(dynamic_cast<E &>(*this));

				if(typeid(*this) == typeid(E))
				{
					guard.pop();
				}
			}
			virtual void react() const noexcept override
			{
				using namespace impl;
				Guard_t &guard = util::TemplateImplRepo::get<ReactKey, Guard_t>();
				if(typeid(*this) == typeid(E))
				{
					guard.emplace();
					guard.top().emplace(typeid(E));
				}
				else if(guard.top().find(typeid(E)) != guard.top().end())
				{
					return;
				}
				else
				{
					guard.top().emplace(typeid(E));
				}

				Unwrapper_t::react(*this);
				Registrar_t::react(dynamic_cast<E const &>(*this));

				if(typeid(*this) == typeid(E))
				{
					guard.pop();
				}
			}

		private:
			Implementor() = default;
			friend EventT;
		};
		template<typename EventT, typename... ParentT>
		Implementor<EventT, ParentT...>::~Implementor<EventT, ParentT...>() = default;

		template<typename EventT, typename... ParentT>
		auto parents(Implementor<EventT, ParentT...> &e) noexcept
		-> typename std::remove_reference<decltype(e)>::type::Parents_t
		{
			return std::remove_reference<decltype(e)>::type::Unwrapper_t::parents(e);
		}
		template<typename EventT, typename... ParentT>
		auto parents(Implementor<EventT, ParentT...> const &e) noexcept
		-> typename std::remove_reference<decltype(e)>::type::ConstParents_t
		{
			return std::remove_reference<decltype(e)>::type::Unwrapper_t::parents(e);
		}
	}
}

#endif
