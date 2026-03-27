/****************************************************************************
 * Copyright (c) 2025 zjlove1989

 * https://github.com/z-jacob/JFramework
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ****************************************************************************/

#ifndef JFRAMEWORK
#define JFRAMEWORK

#include <exception>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace JFramework
{
	// ============================== Exception Classes ==============================

	/**
	 * @brief Base exception class for all framework-related errors
	 *
	 * Inherits from std::runtime_error, serves as the base class for all framework exceptions
	 */
	class FrameworkException : public std::runtime_error
	{
	public:
		using std::runtime_error::runtime_error;
	};

	/**
	 * @brief Exception thrown when architecture is not set
	 *
	 * Thrown when attempting to access components before architecture is initialized
	 */
	class ArchitectureNotSetException : public FrameworkException
	{
	public:
		/**
		 * @brief Constructor
		 * @param typeName Name of the component type that failed to load
		 */
		ArchitectureNotSetException(const std::string& typeName)
			: FrameworkException("Architecture not available: " + typeName)
		{
		}
	};

	/**
	 * @brief Exception thrown when component is not registered
	 *
	 * Thrown when attempting to access an unregistered component
	 */
	class ComponentNotRegisteredException : public FrameworkException
	{
	public:
		/**
		 * @brief Constructor
		 * @param typeName Name of the unregistered component type
		 */
		explicit ComponentNotRegisteredException(const std::string& typeName)
			: FrameworkException("Component not registered: " + typeName)
		{
		}
	};

	/**
	 * @brief Exception thrown when component is already registered
	 *
	 * Thrown when attempting to register a component that already exists
	 */
	class ComponentAlreadyRegisteredException : public FrameworkException
	{
	public:
		/**
		 * @brief Constructor
		 * @param typeName Name of the already registered component type
		 */
		explicit ComponentAlreadyRegisteredException(const std::string& typeName)
			: FrameworkException("Component already registered: " + typeName)
		{
		}
	};

	/**
	 * @brief Exception thrown when command execution fails
	 *
	 * Thrown when a command fails to execute
	 */
	class CommandExecuteException : public FrameworkException
	{
	public:
		/**
		 * @brief Constructor
		 * @param typeName Name of the command type that failed to execute
		 */
		explicit CommandExecuteException(const std::string& typeName)
			: FrameworkException("Command execute Error: " + typeName)
		{
		}
	};

	// ============================== Forward Declarations ==============================

	class ISystem;
	class IModel;
	class IJCommand;
	template <typename _Ty>
	class IQuery;
	class IUtility;
	template <typename _Ty>
	class BindableProperty;
	class IOCContainer;

	/**
	 * @brief Base interface for all events
	 *
	 * All framework events inherit from this class, used for decoupled communication between components
	 */
	class IEvent
	{
	public:
		virtual ~IEvent() = default;
	};

	/**
	 * @brief Interface for event handlers
	 *
	 * Classes implementing this interface can receive and handle events
	 */
	class ICanHandleEvent
	{
	public:
		virtual ~ICanHandleEvent() = default;

		/**
		 * @brief Handle an event
		 * @param event The event to handle
		 */
		virtual void HandleEvent(std::shared_ptr<IEvent> event) = 0;
	};

	/**
	 * @brief Event bus for managing event subscriptions and dispatching
	 *
	 * Implements the publish-subscribe pattern for event-driven communication
	 */
	class EventBus
	{
	public:
		/**
		 * @brief Register an event handler
		 * @param eventType Type index of the event
		 * @param handler Pointer to the event handler
		 */
		void RegisterEvent(std::type_index eventType, ICanHandleEvent* handler)
		{
			std::lock_guard<std::mutex> lock(mMutex);
			mSubscribers[eventType.name()].push_back(handler);
		}

		/**
		 * @brief Send an event to all subscribed handlers
		 * @param event The event to send
		 *
		 * Dispatches the event to all handlers subscribed to this event type
		 */
		void SendEvent(std::shared_ptr<IEvent> event)
		{
			std::vector<ICanHandleEvent*> subscribers;
			{
				std::lock_guard<std::mutex> lock(mMutex);
				auto it = mSubscribers.find(typeid(*event).name());
				if (it != mSubscribers.end())
				{
					subscribers = it->second;
				}
			}

			for (auto& handler : subscribers)
			{
				try
				{
					handler->HandleEvent(event);
				}
				catch (const std::exception&)
				{
				}
			}
		}

		/**
		 * @brief Unregister an event handler
		 * @param eventType Type index of the event
		 * @param handler Pointer to the handler to unregister
		 */
		void UnRegisterEvent(std::type_index eventType, ICanHandleEvent* handler)
		{
			std::lock_guard<std::mutex> lock(mMutex);
			auto it = mSubscribers.find(eventType.name());
			if (it != mSubscribers.end())
			{
				auto& handlers = it->second;
				auto handlerIt = std::find(handlers.begin(), handlers.end(), handler);
				if (handlerIt != handlers.end())
				{
					handlers.erase(handlerIt);
					if (handlers.empty())
					{
						mSubscribers.erase(it);
					}
				}
			}
		}

		/**
		 * @brief Clear all subscriptions
		 */
		void Clear()
		{
			std::lock_guard<std::mutex> lock(mMutex);
			mSubscribers.clear();
		}

	private:
		std::mutex mMutex;                                                          ///< Thread safety mutex
		std::unordered_map<std::string, std::vector<ICanHandleEvent*>> mSubscribers; ///< Event subscriber map
	};

	// ============================== Core Architecture Interface ==============================

	/**
	 * @brief Main architecture interface
	 *
	 * Core interface providing dependency injection and service location, serves as the root container of the framework
	 */
	class IArchitecture : public std::enable_shared_from_this<IArchitecture>
	{
	public:
		virtual ~IArchitecture() = default;

		/**
		 * @brief Get shared_ptr to this object
		 * @return shared_ptr pointing to this object
		 */
		virtual std::shared_ptr<IArchitecture> GetSharedFromThis() = 0;

		/**
		 * @brief Send a command
		 * @param command The command to execute
		 */
		virtual void SendCommand(std::unique_ptr<IJCommand> command) = 0;

		/**
		 * @brief Deinitialize the architecture
		 */
		virtual void Deinit() = 0;

	protected:
		/**
		 * @brief Register a system component
		 * @param typeId Type index
		 * @param system System instance
		 */
		virtual void RegisterSystem(std::type_index typeId,
			std::shared_ptr<ISystem> system)
			= 0;

		/**
		 * @brief Register a model component
		 * @param typeId Type index
		 * @param model Model instance
		 */
		virtual void RegisterModel(std::type_index typeId,
			std::shared_ptr<IModel> model)
			= 0;

		/**
		 * @brief Register a utility component
		 * @param typeId Type index
		 * @param utility Utility instance
		 */
		virtual void RegisterUtility(std::type_index typeId,
			std::shared_ptr<IUtility> utility)
			= 0;

		/**
		 * @brief Get a system component
		 * @param typeId Type index
		 * @return System instance, or nullptr if not found
		 */
		virtual std::shared_ptr<ISystem> GetSystem(std::type_index typeId) = 0;

		/**
		 * @brief Get a model component
		 * @param typeId Type index
		 * @return Model instance, or nullptr if not found
		 */
		virtual std::shared_ptr<IModel> GetModel(std::type_index typeId) = 0;

		/**
		 * @brief Get a utility component
		 * @param typeId Type index
		 * @return Utility instance, or nullptr if not found
		 */
		virtual std::shared_ptr<IUtility> GetUtility(std::type_index typeId) = 0;

		/**
		 * @brief Send an event
		 * @param event The event to send
		 */
		virtual void SendEvent(std::shared_ptr<IEvent> event) = 0;

		/**
		 * @brief Register an event handler
		 * @param eventType Event type index
		 * @param handler Event handler
		 */
		virtual void RegisterEvent(std::type_index eventType,
			ICanHandleEvent* handler)
			= 0;

		/**
		 * @brief Unregister an event handler
		 * @param eventType Event type index
		 * @param handler Event handler
		 */
		virtual void UnRegisterEvent(std::type_index eventType,
			ICanHandleEvent* handler)
			= 0;

	public:
		/**
		 * @brief Template method: Register a system component
		 * @tparam _Ty System type, must inherit from ISystem
		 * @param system System instance
		 */
		template <typename _Ty>
		void RegisterSystem(std::shared_ptr<_Ty> system)
		{
			static_assert(std::is_base_of_v<ISystem, _Ty>,
				"_Ty must inherit from ISystem");
			RegisterSystem(typeid(_Ty), std::static_pointer_cast<ISystem>(system));
		}

		/**
		 * @brief Template method: Register a model component
		 * @tparam _Ty Model type, must inherit from IModel
		 * @param model Model instance
		 */
		template <typename _Ty>
		void RegisterModel(std::shared_ptr<_Ty> model)
		{
			static_assert(std::is_base_of_v<IModel, _Ty>,
				"_Ty must inherit from IModel");
			RegisterModel(typeid(_Ty), std::static_pointer_cast<IModel>(model));
		}

		/**
		 * @brief Template method: Register a utility component
		 * @tparam _Ty Utility type, must inherit from IUtility
		 * @param utility Utility instance
		 */
		template <typename _Ty>
		void RegisterUtility(std::shared_ptr<_Ty> utility)
		{
			if (!utility)
			{
				throw std::invalid_argument("IUtility cannot be null");
			}
			static_assert(std::is_base_of_v<IUtility, _Ty>,
				"_Ty must inherit from IUtility");
			RegisterUtility(typeid(_Ty), std::static_pointer_cast<IUtility>(utility));
		}

		/**
		 * @brief Template method: Get a system component
		 * @tparam _Ty System type
		 * @return System instance
		 * @throws ComponentNotRegisteredException if component is not registered
		 */
		template <typename _Ty>
		std::shared_ptr<_Ty> GetSystem()
		{
			auto system = GetSystem(typeid(_Ty));
			if (!system)
			{
				throw ComponentNotRegisteredException(typeid(_Ty).name());
			}
			return std::dynamic_pointer_cast<_Ty>(system);
		}

		/**
		 * @brief Template method: Get a model component
		 * @tparam _Ty Model type
		 * @return Model instance
		 * @throws ComponentNotRegisteredException if component is not registered
		 */
		template <typename _Ty>
		std::shared_ptr<_Ty> GetModel()
		{
			auto model = GetModel(typeid(_Ty));
			if (!model)
			{
				throw ComponentNotRegisteredException(typeid(_Ty).name());
			}
			return std::dynamic_pointer_cast<_Ty>(model);
		}

		/**
		 * @brief Template method: Get a utility component
		 * @tparam _Ty Utility type
		 * @return Utility instance
		 * @throws ComponentNotRegisteredException if component is not registered
		 */
		template <typename _Ty>
		std::shared_ptr<_Ty> GetUtility()
		{
			auto utility = GetUtility(typeid(_Ty));
			if (!utility)
			{
				throw ComponentNotRegisteredException(typeid(_Ty).name());
			}
			return std::dynamic_pointer_cast<_Ty>(utility);
		}

		/**
		 * @brief Template method: Register an event handler
		 * @tparam _Ty Event type, must inherit from IEvent
		 * @param handler Event handler pointer
		 */
		template <typename _Ty>
		void RegisterEvent(ICanHandleEvent* handler)
		{
			if (!handler)
			{
				throw std::invalid_argument("ICanHandleEvent cannot be null");
			}
			static_assert(std::is_base_of_v<IEvent, _Ty>,
				"_Ty must inherit from IEvent");
			mEventBus->RegisterEvent(typeid(_Ty), handler);
		}

		/**
		 * @brief Template method: Unregister an event handler
		 * @tparam _Ty Event type, must inherit from IEvent
		 * @param handler Event handler pointer
		 */
		template <typename _Ty>
		void UnRegisterEvent(ICanHandleEvent* handler)
		{
			if (!handler)
			{
				throw std::invalid_argument("ICanHandleEvent cannot be null");
			}
			static_assert(std::is_base_of_v<IEvent, _Ty>,
				"_Ty must inherit from IEvent");
			mEventBus->UnRegisterEvent(typeid(_Ty), handler);
		}

		/**
		 * @brief Template method: Send an event
		 * @tparam _Ty Event type, must inherit from IEvent
		 * @tparam Args Constructor argument types
		 * @param args Event constructor arguments
		 */
		template <typename _Ty, typename... Args>
		void SendEvent(Args&&... args)
		{
			static_assert(std::is_base_of_v<IEvent, _Ty>,
				"_Ty must inherit from IEvent");
			this->SendEvent(std::make_shared<_Ty>(std::forward<Args>(args)...));
		}

		/**
		 * @brief Template method: Send a command
		 * @tparam _Ty Command type, must inherit from IJCommand
		 * @tparam Args Constructor argument types
		 * @param args Command constructor arguments
		 */
		template <typename _Ty, typename... Args>
		void SendCommand(Args&&... args)
		{
			static_assert(std::is_base_of_v<IJCommand, _Ty>,
				"_Ty must inherit from ICommand");
			this->SendCommand(std::make_unique<_Ty>(std::forward<Args>(args)...));
		}

		/**
		 * @brief Template method: Send a query and return the result
		 * @tparam _Ty Query type
		 * @param query Query instance
		 * @return Query result
		 */
		template <typename _Ty>
		auto SendQuery(std::unique_ptr<_Ty> query) -> decltype(query->Do())
		{
			static_assert(std::is_base_of_v<IQuery<decltype(query->Do())>, _Ty>,
				"_Ty must inherit from IQuery");

			if (!query)
			{
				throw std::invalid_argument("Query cannot be null");
			}
			query->SetArchitecture(GetSharedFromThis());
			return query->Do();
		}

		/**
		 * @brief Template method: Create and send a query
		 * @tparam _Ty Query type
		 * @tparam Args Constructor argument types
		 * @param args Query constructor arguments
		 * @return Query result
		 */
		template <typename _Ty, typename... Args>
		auto SendQuery(Args&&... args) -> decltype(std::declval<_Ty>().Do())
		{
			static_assert(
				std::is_base_of_v<IQuery<decltype(std::declval<_Ty>().Do())>, _Ty>,
				"_Ty must inherit from IQuery");

			auto query = std::make_unique<_Ty>(std::forward<Args>(args)...);
			return this->SendQuery(std::move(query));
		}

		/**
		 * @brief Check if architecture is initialized
		 * @return true if initialized
		 */
		bool IsInitialized() const { return mInitialized; }

	protected:
		bool mInitialized = false;                 ///< Initialization state flag
		std::unique_ptr<IOCContainer> mContainer;  ///< IoC container
		std::unique_ptr<EventBus> mEventBus;       ///< Event bus
	};

	// ============================== Registration Interfaces ==============================

	/**
	 * @brief Unregister interface
	 *
	 * Base interface for managing unregisterable resources
	 */
	class IUnRegister
	{
	public:
		virtual ~IUnRegister() = default;

		/**
		 * @brief Execute unregister operation
		 */
		virtual void UnRegister() = 0;
	};

	/**
	 * @brief Unregister trigger
	 *
	 * Automatically triggers all registered unregister operations on destruction
	 */
	class UnRegisterTrigger
	{
	public:
		virtual ~UnRegisterTrigger() { this->UnRegister(); }

		/**
		 * @brief Add an unregister object
		 * @param unRegister Unregister object to add
		 */
		void AddUnRegister(std::shared_ptr<IUnRegister> unRegister)
		{
			std::lock_guard<std::mutex> lock(mMutex);
			mUnRegisters.push_back(std::move(unRegister));
		}

		/**
		 * @brief Execute all unregister operations
		 */
		void UnRegister()
		{
			std::lock_guard<std::mutex> lock(mMutex);
			for (auto& unRegister : mUnRegisters)
			{
				unRegister->UnRegister();
			}
			mUnRegisters.clear();
		}

	protected:
		std::mutex mMutex;                                      ///< Thread safety mutex
		std::vector<std::shared_ptr<IUnRegister>> mUnRegisters; ///< List of unregister objects
	};

	/**
	 * @brief Bindable property unregister handler
	 *
	 * Manages observer unregistration for bindable properties
	 * @tparam _Ty Property value type
	 */
	template <typename _Ty>
	class BindablePropertyUnRegister
		: public IUnRegister,
		public std::enable_shared_from_this<BindablePropertyUnRegister<_Ty>>
	{
	public:
		/**
		 * @brief Constructor
		 * @param id Observer ID
		 * @param property Associated bindable property
		 * @param callback Value change callback function
		 */
		BindablePropertyUnRegister(int id,
			BindableProperty<_Ty>* property,
			std::function<void(_Ty)> callback)
			: mProperty(property)
			, mCallback(std::move(callback))
			, mId(id)
		{
		}

		/**
		 * @brief Set auto-unregister on object destruction
		 * @param unRegisterTrigger Unregister trigger
		 */
		void UnRegisterWhenObjectDestroyed(UnRegisterTrigger* unRegisterTrigger)
		{
			unRegisterTrigger->AddUnRegister(this->shared_from_this());
		}

		/**
		 * @brief Get observer ID
		 * @return Observer ID
		 */
		int GetId() const { return mId; }

		/**
		 * @brief Set associated bindable property
		 * @param property Property pointer
		 */
		void SetProperty(BindableProperty<_Ty>* property)
		{
			mProperty = property;
		}

		/**
		 * @brief Execute unregister operation
		 */
		void UnRegister() override
		{
			if (mProperty)
			{
				mProperty->UnRegister(mId);
				mProperty = nullptr;
			}
		}

		/**
		 * @brief Invoke callback function
		 * @param value New value
		 */
		void Invoke(_Ty value)
		{
			if (mCallback)
			{
				mCallback(std::move(value));
			}
		}

	protected:
		int mId;  ///< Observer ID

	private:
		BindableProperty<_Ty>* mProperty;    ///< Associated bindable property
		std::function<void(_Ty)> mCallback;  ///< Value change callback function
	};

	/**
	 * @brief Bindable property template class
	 *
	 * Observable property implementing the observer pattern, supports value change notifications
	 * @tparam _Ty Property value type
	 */
	template <typename _Ty>
	class BindableProperty
	{
	public:
		BindableProperty() = default;

		/// Copy construction is disabled
		BindableProperty(const BindableProperty&) = delete;

		/// Copy assignment is disabled
		BindableProperty& operator=(const BindableProperty&) = delete;

		/**
		 * @brief Move constructor
		 * @param other Source object
		 */
		BindableProperty(BindableProperty&& other) noexcept
		{
			std::lock_guard<std::mutex> lock(other.mMutex);
			mValue = std::move(other.mValue);
			mObservers = std::move(other.mObservers);
			mNextId = other.mNextId;
			for (auto& observer : mObservers)
			{
				if (observer)
					observer->SetProperty(this);
			}
			other.mNextId = 0;
		}

		/**
		 * @brief Move assignment operator
		 * @param other Source object
		 * @return Reference to this object
		 */
		BindableProperty& operator=(BindableProperty&& other) noexcept
		{
			if (this != &other)
			{
				std::lock_guard<std::mutex> lock1(mMutex);
				std::lock_guard<std::mutex> lock2(other.mMutex);
				mValue = std::move(other.mValue);
				mObservers = std::move(other.mObservers);
				mNextId = other.mNextId;
				for (auto& observer : mObservers)
				{
					if (observer)
						observer->SetProperty(this);
				}
				other.mNextId = 0;
			}
			return *this;
		}

		/**
		 * @brief Constructor with initial value
		 * @param value Initial value
		 */
		explicit BindableProperty(const _Ty& value)
			: mValue(std::move(value))
		{
		}

		/**
		 * @brief Destructor, clears all observers
		 */
		~BindableProperty()
		{
			std::lock_guard<std::mutex> lock(mMutex);
			mObservers.clear();
		}

		/**
		 * @brief Get current value
		 * @return Current value
		 */
		const _Ty& GetValue() const { return mValue; }

		/**
		 * @brief Set new value and notify observers
		 * @param newValue New value
		 */
		void SetValue(const _Ty& newValue)
		{
			std::lock_guard<std::mutex> lock(mMutex);
			if (mValue == newValue)
				return;
			mValue = newValue;
			for (auto& observer : mObservers)
			{
				try
				{
					observer->Invoke(mValue);
				}
				catch (const std::exception&)
				{

				}
			}
		}

		/**
		 * @brief Set new value without triggering notification
		 * @param newValue New value
		 */
		void SetValueWithoutEvent(const _Ty& newValue)
		{
			std::lock_guard<std::mutex> lock(mMutex);
			mValue = newValue;
		}

		/**
		 * @brief Register observer and send initial value notification
		 * @param onValueChanged Value change callback function
		 * @return Unregister object
		 */
		std::shared_ptr<BindablePropertyUnRegister<_Ty>> RegisterWithInitValue(
			std::function<void(const _Ty&)> onValueChanged)
		{
			onValueChanged(mValue);
			return Register(std::move(onValueChanged));
		}

		/**
		 * @brief Register observer
		 * @param onValueChanged Value change callback function
		 * @return Unregister object
		 */
		std::shared_ptr<BindablePropertyUnRegister<_Ty>> Register(
			std::function<void(const _Ty&)> onValueChanged)
		{
			std::lock_guard<std::mutex> lock(mMutex);
			auto unRegister = std::make_shared<BindablePropertyUnRegister<_Ty>>(
				mNextId++, this, std::move(onValueChanged));
			mObservers.push_back(unRegister);
			return unRegister;
		}

		/**
		 * @brief Unregister observer
		 * @param id Observer ID
		 */
		void UnRegister(int id)
		{
			std::lock_guard<std::mutex> lock(mMutex);
			for (size_t i = 0; i < mObservers.size(); i++)
			{
				auto& observer = mObservers[i];
				if (observer && observer->GetId() == id)
				{
					mObservers.erase(mObservers.begin() + i);
					break;
				}
			}
		}

		/// Type conversion operator
		operator _Ty() const { return mValue; }

		/**
		 * @brief Assignment operator
		 * @param newValue New value
		 * @return Reference to this object
		 */
		BindableProperty<_Ty>& operator=(const _Ty& newValue)
		{
			SetValue(std::move(newValue));
			return *this;
		}

	private:
		std::mutex mMutex;       ///< Thread safety mutex
		int mNextId = 0;         ///< Next observer ID
		_Ty mValue;              ///< Property value
		std::vector<std::shared_ptr<BindablePropertyUnRegister<_Ty>>> mObservers; ///< Observer list
	};

	// ============================== Capability Interfaces ==============================

	/**
	 * @brief Initialization management interface
	 *
	 * Provides initialization and deinitialization capabilities
	 */
	class ICanInit
	{
	public:
		/**
		 * @brief Check if initialized
		 * @return true if initialized
		 */
		bool IsInitialized() const { return mInitialized; }

		/**
		 * @brief Set initialization state
		 * @param initialized Initialization state
		 */
		void SetInitialized(bool initialized) { mInitialized = initialized; }

		virtual ~ICanInit() = default;

		/**
		 * @brief Initialize
		 */
		virtual void Init() = 0;

		/**
		 * @brief Deinitialize
		 */
		virtual void Deinit() = 0;

	protected:
		bool mInitialized = false; ///< Initialization state flag
	};

	/**
	 * @brief Architecture ownership interface
	 *
	 * Classes implementing this interface can access their owning architecture instance
	 */
	class IBelongToArchitecture
	{
	public:
		virtual ~IBelongToArchitecture() = default;

		/**
		 * @brief Get owning architecture
		 * @return Weak reference to the architecture
		 */
		virtual std::weak_ptr<IArchitecture> GetArchitecture() const = 0;
	};

	/**
	 * @brief Architecture setting interface
	 *
	 * Classes implementing this interface can have their owning architecture set
	 */
	class ICanSetArchitecture
	{
	public:
		virtual ~ICanSetArchitecture() = default;

		/**
		 * @brief Set owning architecture
		 * @param architecture Architecture instance
		 */
		virtual void SetArchitecture(std::shared_ptr<IArchitecture> architecture) = 0;
	};

	// ============================== Component Access Interfaces ==============================

	/**
	 * @brief Model access interface
	 *
	 * Classes implementing this interface can access model components
	 */
	class ICanGetModel : public IBelongToArchitecture
	{
	public:
		/**
		 * @brief Get a model component
		 * @tparam _Ty Model type
		 * @return Model instance
		 */
		template <typename _Ty>
		std::shared_ptr<_Ty> GetModel()
		{
			static_assert(std::is_base_of_v<IModel, _Ty>,
				"_Ty must inherit from IModel");

			auto arch = GetArchitecture().lock();
			if (!arch)
			{
				throw ArchitectureNotSetException(typeid(_Ty).name());
			}

			auto model = arch->GetModel<_Ty>();
			return model;
		}
	};

	/**
	 * @brief System access interface
	 *
	 * Classes implementing this interface can access system components
	 */
	class ICanGetSystem : public IBelongToArchitecture
	{
	public:
		/**
		 * @brief Get a system component
		 * @tparam _Ty System type
		 * @return System instance
		 */
		template <typename _Ty>
		std::shared_ptr<_Ty> GetSystem()
		{
			static_assert(std::is_base_of_v<ISystem, _Ty>,
				"_Ty must inherit from ISystem");

			auto arch = GetArchitecture().lock();
			if (!arch)
			{
				throw ArchitectureNotSetException(typeid(_Ty).name());
			}

			auto system = arch->GetSystem<_Ty>();
			return system;
		}
	};

	/**
	 * @brief Command sending interface
	 *
	 * Classes implementing this interface can send commands
	 */
	class ICanSendCommand : public IBelongToArchitecture
	{
	public:
		/**
		 * @brief Send a command
		 * @tparam _Ty Command type
		 * @tparam Args Constructor argument types
		 * @param args Command constructor arguments
		 */
		template <typename _Ty, typename... Args>
		void SendCommand(Args&&... args)
		{
			static_assert(std::is_base_of_v<IJCommand, _Ty>,
				"_Ty must inherit from IJCommand");

			auto arch = GetArchitecture().lock();
			if (!arch)
			{
				throw ArchitectureNotSetException(typeid(_Ty).name());
			}
			arch->SendCommand<_Ty>(std::forward<Args>(args)...);
		}

		/**
		 * @brief Send a command
		 * @param command Command instance
		 */
		void SendCommand(std::unique_ptr<IJCommand> command)
		{

			auto arch = GetArchitecture().lock();
			if (!arch)
			{
				throw ArchitectureNotSetException(typeid(command).name());
			}
			arch->SendCommand(std::move(command));
		}
	};

	/**
	 * @brief Query sending interface
	 *
	 * Classes implementing this interface can send queries
	 */
	class ICanSendQuery : public IBelongToArchitecture
	{
	public:
		/**
		 * @brief Send a query
		 * @tparam _Ty Query type
		 * @tparam Args Constructor argument types
		 * @param args Query constructor arguments
		 * @return Query result
		 */
		template <typename _Ty, typename... Args>
		auto SendQuery(Args&&... args) -> decltype(std::declval<_Ty>().Do())
		{
			auto arch = GetArchitecture().lock();
			if (!arch)
			{
				throw ArchitectureNotSetException(typeid(_Ty).name());
			}

			auto result = arch->SendQuery<_Ty>(std::forward<Args>(args)...);
			return result;
		}

		/**
		 * @brief Send a query
		 * @tparam _Ty Query type
		 * @param query Query instance
		 * @return Query result
		 */
		template <typename _Ty>
		auto SendQuery(std::unique_ptr<_Ty> query)
			-> decltype(std::declval<_Ty>().Do())
		{
			auto arch = GetArchitecture().lock();
			if (!arch)
			{
				throw ArchitectureNotSetException(typeid(_Ty).name());
			}

			auto result = arch->SendQuery(std::move(query));
			return result;
		}
	};

	/**
	 * @brief Utility access interface
	 *
	 * Classes implementing this interface can access utility components
	 */
	class ICanGetUtility : public IBelongToArchitecture
	{
	public:
		/**
		 * @brief Get a utility component
		 * @tparam _Ty Utility type
		 * @return Utility instance
		 */
		template <typename _Ty>
		std::shared_ptr<_Ty> GetUtility()
		{
			static_assert(std::is_base_of_v<IUtility, _Ty>,
				"_Ty must inherit from IUtility");

			auto arch = GetArchitecture().lock();
			if (!arch)
			{
				throw ArchitectureNotSetException(typeid(_Ty).name());
			}

			auto utility = arch->GetUtility<_Ty>();
			return utility;
		}
	};

	// ============================== Event Interfaces ==============================

	/**
	 * @brief Event sending interface
	 *
	 * Classes implementing this interface can send events
	 */
	class ICanSendEvent : public IBelongToArchitecture
	{
	public:
		/**
		 * @brief Send an event
		 * @tparam _Ty Event type
		 * @tparam Args Constructor argument types
		 * @param args Event constructor arguments
		 */
		template <typename _Ty, typename... Args>
		void SendEvent(Args&&... args)
		{
			static_assert(std::is_base_of_v<IEvent, _Ty>,
				"_Ty must inherit from IEvent");

			auto arch = GetArchitecture().lock();
			if (!arch)
			{
				throw ArchitectureNotSetException(typeid(_Ty).name());
			}
			arch->SendEvent<_Ty>(std::forward<Args>(args)...);
		}
	};

	/**
	 * @brief Event registration interface
	 *
	 * Classes implementing this interface can register and unregister event handlers
	 */
	class ICanRegisterEvent : public IBelongToArchitecture
	{
	public:
		virtual ~ICanRegisterEvent() = default;

		/**
		 * @brief Register an event handler
		 * @tparam _Ty Event type
		 * @param handler Event handler
		 */
		template <typename _Ty>
		void RegisterEvent(ICanHandleEvent* handler)
		{
			static_assert(std::is_base_of_v<IEvent, _Ty>,
				"_Ty must inherit from IEvent");

			auto arch = GetArchitecture().lock();
			if (!arch)
			{
				throw ArchitectureNotSetException(typeid(_Ty).name());
			}

			arch->RegisterEvent<_Ty>(handler);
		}

		/**
		 * @brief Unregister an event handler
		 * @tparam _Ty Event type
		 * @param handler Event handler
		 */
		template <typename _Ty>
		void UnRegisterEvent(ICanHandleEvent* handler)
		{
			static_assert(std::is_base_of_v<IEvent, _Ty>,
				"_Ty must inherit from IEvent");

			auto arch = GetArchitecture().lock();
			if (!arch)
			{
				throw ArchitectureNotSetException(typeid(_Ty).name());
			}

			arch->UnRegisterEvent<_Ty>(handler);
		}
	};

	// ============================== Core Component Interfaces ==============================

	/**
	 * @brief Command interface
	 *
	 * Base interface for the command pattern, used to encapsulate operation requests
	 */
	class IJCommand : public ICanSetArchitecture,
		public ICanGetSystem,
		public ICanGetModel,
		public ICanSendCommand,
		public ICanSendEvent,
		public ICanSendQuery,
		public ICanGetUtility
	{
	public:
		virtual ~IJCommand() override = default;

		/**
		 * @brief Execute the command
		 */
		virtual void Execute() = 0;
	};

	/**
	 * @brief Model interface
	 *
	 * Base interface for data models, used to manage data and state
	 */
	class IModel : public ICanSetArchitecture,
		public ICanInit,
		public ICanSendEvent,
		public ICanSendQuery,
		public ICanGetUtility,
		public ICanGetModel
	{
	};

	/**
	 * @brief System interface
	 *
	 * Base interface for system components, used to implement business logic and handle events
	 */
	class ISystem : public ICanSetArchitecture,
		public ICanInit,
		public ICanGetModel,
		public ICanHandleEvent,
		public ICanGetSystem,
		public ICanRegisterEvent,
		public ICanSendEvent,
		public ICanSendQuery,
		public ICanGetUtility
	{
	};

	/**
	 * @brief Controller interface
	 *
	 * Base interface for controllers, used to coordinate interactions between systems, models, and views
	 */
	class IController : public ICanGetSystem,
		public ICanGetModel,
		public ICanSendCommand,
		public ICanSendEvent,
		public ICanHandleEvent,
		public ICanRegisterEvent,
		public ICanSendQuery,
		public ICanGetUtility
	{
	};

	/**
	 * @brief Query interface
	 *
	 * Base interface for the query pattern, used to encapsulate data query requests
	 * @tparam _Ty Query result type
	 */
	template <typename _Ty>
	class IQuery : public ICanSetArchitecture,
		public ICanGetModel,
		public ICanGetSystem,
		public ICanSendQuery
	{
	public:
		virtual ~IQuery() override = default;

		/**
		 * @brief Execute the query
		 * @return Query result
		 */
		virtual _Ty Do() = 0;
	};

	/**
	 * @brief Utility interface
	 *
	 * Base interface for utility components, used to provide common functionality services
	 */
	class IUtility
	{
	public:
		virtual ~IUtility() = default;
	};

	// ============================== Implementation Classes ==============================

	/**
	 * @brief Inversion of Control container
	 *
	 * Core container implementing dependency injection, manages component registration and retrieval
	 */
	class IOCContainer
	{
	public:
		/**
		 * @brief Register a component
		 * @tparam _Ty Component type
		 * @tparam TBase Component base type
		 * @param typeId Type index
		 * @param component Component instance
		 */
		template <typename _Ty, typename TBase>
		void Register(std::type_index typeId, std::shared_ptr<TBase> component)
		{
			static_assert(std::is_base_of_v<TBase, _Ty>, "_Ty must inherit from TBase");
			auto& container = GetContainer(ContainerTypeTag<TBase> {});
			std::lock_guard<std::mutex> lock(GetMutex(MutexTypeTag<TBase> {}));

			if (container.find(typeId.name()) != container.end())
				throw ComponentAlreadyRegisteredException(typeId.name());

			container[typeId.name()] = std::static_pointer_cast<TBase>(component);
		}

		/**
		 * @brief Get a component
		 * @tparam TBase Component base type
		 * @param typeId Type index
		 * @return Component instance, or nullptr if not found
		 */
		template <typename TBase>
		std::shared_ptr<TBase> Get(std::type_index typeId)
		{
			auto& container = GetContainer(ContainerTypeTag<TBase> {});
			std::lock_guard<std::mutex> lock(GetMutex(MutexTypeTag<TBase> {}));
			auto it = container.find(typeId.name());
			return it != container.end() ? it->second : nullptr;
		}

		/**
		 * @brief Get all components of a type
		 * @tparam TBase Component base type
		 * @return List of components
		 */
		template <typename TBase>
		std::vector<std::shared_ptr<TBase>> GetAll()
		{
			auto& container = GetContainer(ContainerTypeTag<TBase> {});
			std::lock_guard<std::mutex> lock(GetMutex(MutexTypeTag<TBase> {}));
			std::vector<std::shared_ptr<TBase>> result;
			for (auto& pair : container)
			{
				result.push_back(pair.second);
			}
			return result;
		}

		/**
		 * @brief Clear all components
		 */
		void Clear()
		{
			std::lock_guard<std::mutex> lock1(mModelMutex);
			std::lock_guard<std::mutex> lock2(mSystemMutex);
			std::lock_guard<std::mutex> lock3(mUtilityMutex);
			mModels.clear();
			mSystems.clear();
			mUtilitys.clear();
		}

	private:
		/// Container type tag
		template <typename>
		struct ContainerTypeTag {};

		auto& GetContainer(ContainerTypeTag<IModel>) { return mModels; }
		auto& GetContainer(ContainerTypeTag<ISystem>) { return mSystems; }
		auto& GetContainer(ContainerTypeTag<IUtility>) { return mUtilitys; }

		/// Mutex type tag
		template <typename>
		struct MutexTypeTag {};

		auto& GetMutex(MutexTypeTag<IModel>) { return mModelMutex; }
		auto& GetMutex(MutexTypeTag<ISystem>) { return mSystemMutex; }
		auto& GetMutex(MutexTypeTag<IUtility>) { return mUtilityMutex; }

		std::unordered_map<std::string, std::shared_ptr<IModel>> mModels;     ///< Model container
		std::unordered_map<std::string, std::shared_ptr<ISystem>> mSystems;   ///< System container
		std::unordered_map<std::string, std::shared_ptr<IUtility>> mUtilitys; ///< Utility container

		std::mutex mModelMutex;   ///< Model container mutex
		std::mutex mSystemMutex;  ///< System container mutex
		std::mutex mUtilityMutex; ///< Utility container mutex
	};

	/**
	 * @brief Architecture implementation class
	 *
	 * Core architecture implementation managing components and services
	 */
	class Architecture : public IArchitecture
	{
	public:
		using IArchitecture::GetModel;
		using IArchitecture::GetSystem;
		using IArchitecture::GetUtility;
		using IArchitecture::RegisterEvent;
		using IArchitecture::RegisterModel;
		using IArchitecture::RegisterSystem;
		using IArchitecture::RegisterUtility;
		using IArchitecture::SendCommand;
		using IArchitecture::SendEvent;
		using IArchitecture::UnRegisterEvent;

	private:
		/**
		 * @brief Register a system component
		 * @param typeId Type index
		 * @param system System instance
		 */
		void RegisterSystem(std::type_index typeId,
			std::shared_ptr<ISystem> system) override
		{
			if (!system)
			{
				throw std::invalid_argument("System cannot be null");
			}
			system->SetArchitecture(shared_from_this());
			mContainer->Register<ISystem>(typeId, system);
			if (mInitialized)
			{
				InitializeComponent(system);
			}
		}

		/**
		 * @brief Get a system component
		 * @param typeId Type index
		 * @return System instance
		 */
		std::shared_ptr<ISystem> GetSystem(std::type_index typeId) override
		{
			return mContainer->Get<ISystem>(typeId);
		}

		/**
		 * @brief Register a model component
		 * @param typeId Type index
		 * @param model Model instance
		 */
		void RegisterModel(std::type_index typeId,
			std::shared_ptr<IModel> model) override
		{
			if (!model)
			{
				throw std::invalid_argument("Model cannot be null");
			}
			model->SetArchitecture(shared_from_this());
			mContainer->Register<IModel>(typeId, model);
			if (mInitialized)
			{
				InitializeComponent(model);
			}
		}

		/**
		 * @brief Get a model component
		 * @param typeId Type index
		 * @return Model instance
		 */
		std::shared_ptr<IModel> GetModel(std::type_index typeId) override
		{
			return mContainer->Get<IModel>(typeId);
		}

		/**
		 * @brief Register a utility component
		 * @param typeId Type index
		 * @param utility Utility instance
		 */
		void RegisterUtility(std::type_index typeId,
			std::shared_ptr<IUtility> utility) override
		{
			mContainer->Register<IUtility>(typeId, utility);
		}

		/**
		 * @brief Get a utility component
		 * @param typeId Type index
		 * @return Utility instance
		 */
		std::shared_ptr<IUtility> GetUtility(std::type_index typeId) override
		{
			return mContainer->Get<IUtility>(typeId);
		}

		/**
		 * @brief Register an event handler
		 * @param eventType Event type index
		 * @param handler Event handler
		 */
		void RegisterEvent(std::type_index eventType,
			ICanHandleEvent* handler) override
		{
			if (!handler)
			{
				throw std::invalid_argument("ICanHandleEvent cannot be null");
			}
			mEventBus->RegisterEvent(eventType, handler);
		}

		/**
		 * @brief Unregister an event handler
		 * @param eventType Event type index
		 * @param handler Event handler
		 */
		void UnRegisterEvent(std::type_index eventType,
			ICanHandleEvent* handler) override
		{
			if (!handler)
			{
				throw std::invalid_argument("ICanHandleEvent cannot be null");
			}
			mEventBus->UnRegisterEvent(eventType, handler);
		}

	public:
		/**
		 * @brief Get shared_ptr to this object
		 * @return shared_ptr pointing to this object
		 */
		std::shared_ptr<IArchitecture> GetSharedFromThis() final
		{
			return shared_from_this();
		}

		/**
		 * @brief Send a command
		 * @param command Command instance
		 */
		void SendCommand(std::unique_ptr<IJCommand> command) override
		{
			if (!command)
			{
				throw std::invalid_argument("ICommand cannot be null");
			}
			command->SetArchitecture(shared_from_this());
			command->Execute();
		}

		/**
		 * @brief Send an event
		 * @param event Event instance
		 */
		void SendEvent(std::shared_ptr<IEvent> event) override
		{
			if (!event)
			{
				throw std::invalid_argument("IEvent cannot be null");
			}
			mEventBus->SendEvent(event);
		}

		/**
		 * @brief Deinitialize the architecture
		 */
		void Deinit() final
		{
			if (!mInitialized)
				return;

			mInitialized = false;

			this->OnDeinit();

			for (auto& model : mContainer->GetAll<IModel>())
			{
				UnInitializeComponent(model);
			}

			for (auto& system : mContainer->GetAll<ISystem>())
			{
				UnInitializeComponent(system);
			}
		}

		/**
		 * @brief Initialize the architecture
		 *
		 * Initializes all registered model and system components
		 */
		virtual void InitArchitecture()
		{
			if (mInitialized)
				return;

			mInitialized = true;

			this->Init();

			for (auto& model : mContainer->GetAll<IModel>())
			{
				InitializeComponent(model);
			}

			for (auto& system : mContainer->GetAll<ISystem>())
			{
				InitializeComponent(system);
			}
		}

		/**
		 * @brief Get the IoC container
		 * @return Pointer to the IoC container
		 */
		IOCContainer* GetContainer() { return mContainer.get(); }

	protected:
		/**
		 * @brief Protected constructor
		 */
		Architecture()
		{
			mContainer = std::make_unique<IOCContainer>();
			mEventBus = std::make_unique<EventBus>();
			mInitialized = false;
		}

		virtual ~Architecture() = default;

		/**
		 * @brief Initialization callback, implemented by subclasses
		 */
		virtual void Init() = 0;

		/**
		 * @brief Deinitialization callback, implemented by subclasses
		 */
		virtual void OnDeinit() {}

	private:
		/**
		 * @brief Initialize a component
		 * @tparam _Ty Component type
		 * @param component Component instance
		 */
		template <typename _Ty>
		void InitializeComponent(std::shared_ptr<_Ty> component)
		{
			static_assert(std::is_base_of_v<ICanInit, _Ty>,
				"Component must implement ICanInit");

			if (!component->IsInitialized())
			{
				component->Init();
				component->SetInitialized(true);
			}
		}

		/**
		 * @brief Deinitialize a component
		 * @tparam _Ty Component type
		 * @param component Component instance
		 */
		template <typename _Ty>
		void UnInitializeComponent(std::shared_ptr<_Ty> component)
		{
			static_assert(std::is_base_of_v<ICanInit, _Ty>,
				"Component must implement ICanInit");

			if (component->IsInitialized())
			{
				component->Deinit();
				component->SetInitialized(false);
			}
		}
	};

	// ============================== Abstract Base Classes ==============================

	/**
	 * @brief Abstract command base class
	 *
	 * Provides basic command implementation, subclasses only need to implement OnExecute method
	 */
	class AbstractCommand : public IJCommand
	{
	private:
		std::weak_ptr<IArchitecture> mArchitecture; ///< Weak reference to architecture

	public:
		/**
		 * @brief Get owning architecture
		 * @return Weak reference to architecture
		 */
		std::weak_ptr<IArchitecture> GetArchitecture() const final
		{
			return mArchitecture;
		}

		/**
		 * @brief Execute the command
		 */
		void Execute() final { this->OnExecute(); }

	public:
		/**
		 * @brief Set owning architecture
		 * @param architecture Architecture instance
		 */
		void SetArchitecture(std::shared_ptr<IArchitecture> architecture) final
		{
			mArchitecture = architecture;
		}

	protected:
		/**
		 * @brief Command execution implementation
		 */
		virtual void OnExecute() = 0;
	};

	/**
	 * @brief Abstract model base class
	 *
	 * Provides basic model implementation, subclasses need to implement OnInit and OnDeinit methods
	 */
	class AbstractModel : public virtual IModel
	{
	private:
		std::weak_ptr<IArchitecture> mArchitecture; ///< Weak reference to architecture

	public:
		/**
		 * @brief Get owning architecture
		 * @return Weak reference to architecture
		 */
		std::weak_ptr<IArchitecture> GetArchitecture() const final
		{
			return mArchitecture;
		}

		/**
		 * @brief Initialize
		 */
		virtual void Init() final { this->OnInit(); }

		/**
		 * @brief Deinitialize
		 */
		void Deinit() final { this->OnDeinit(); }

	private:
		/**
		 * @brief Set owning architecture
		 * @param architecture Architecture instance
		 */
		void SetArchitecture(std::shared_ptr<IArchitecture> architecture) final
		{
			mArchitecture = architecture;
		}

	protected:
		/**
		 * @brief Initialization implementation
		 */
		virtual void OnInit() = 0;

		/**
		 * @brief Deinitialization implementation
		 */
		virtual void OnDeinit() = 0;
	};

	/**
	 * @brief Abstract system base class
	 *
	 * Provides basic system implementation, subclasses need to implement OnInit, OnDeinit and OnEvent methods
	 */
	class AbstractSystem : public virtual ISystem
	{
	private:
		std::weak_ptr<IArchitecture> mArchitecture; ///< Weak reference to architecture

	public:
		/**
		 * @brief Get owning architecture
		 * @return Weak reference to architecture
		 */
		std::weak_ptr<IArchitecture> GetArchitecture() const final
		{
			return mArchitecture;
		}

		/**
		 * @brief Initialize
		 */
		virtual void Init() final { this->OnInit(); }

		/**
		 * @brief Deinitialize
		 */
		void Deinit() final { OnDeinit(); }

		/**
		 * @brief Handle event
		 * @param event Event instance
		 */
		void HandleEvent(std::shared_ptr<IEvent> event) final { OnEvent(event); }

	private:
		/**
		 * @brief Set owning architecture
		 * @param architecture Architecture instance
		 */
		void SetArchitecture(std::shared_ptr<IArchitecture> architecture) final
		{
			mArchitecture = architecture;
		}

	protected:
		/**
		 * @brief Initialization implementation
		 */
		virtual void OnInit() = 0;

		/**
		 * @brief Deinitialization implementation
		 */
		virtual void OnDeinit() = 0;

		/**
		 * @brief Event handling implementation
		 * @param event Event instance
		 */
		virtual void OnEvent(std::shared_ptr<IEvent> event) = 0;
	};

	/**
	 * @brief Abstract controller base class
	 *
	 * Provides basic controller implementation, subclasses need to implement OnEvent method
	 */
	class AbstractController : public IController
	{
	private:
		/**
		 * @brief Handle event
		 * @param event Event instance
		 */
		void HandleEvent(std::shared_ptr<IEvent> event) final { OnEvent(event); }

	protected:
		/**
		 * @brief Event handling implementation
		 * @param event Event instance
		 */
		virtual void OnEvent(std::shared_ptr<IEvent> event) = 0;
	};

	/**
	 * @brief Abstract query base class
	 *
	 * Provides basic query implementation, subclasses only need to implement OnDo method
	 * @tparam _Ty Query result type
	 */
	template <typename _Ty>
	class AbstractQuery : public IQuery<_Ty>
	{
	private:
		std::weak_ptr<IArchitecture> mArchitecture; ///< Weak reference to architecture

	public:
		/**
		 * @brief Get owning architecture
		 * @return Weak reference to architecture
		 */
		std::weak_ptr<IArchitecture> GetArchitecture() const final
		{
			return mArchitecture;
		}

		/**
		 * @brief Execute the query
		 * @return Query result
		 */
		_Ty Do() final { return OnDo(); }

	public:
		/**
		 * @brief Set owning architecture
		 * @param architecture Architecture instance
		 */
		void SetArchitecture(std::shared_ptr<IArchitecture> architecture) final
		{
			mArchitecture = architecture;
		}

	protected:
		/**
		 * @brief Query execution implementation
		 * @return Query result
		 */
		virtual _Ty OnDo() = 0;
	};
}; // namespace JFramework

#endif // !_JFRAMEWORK_
