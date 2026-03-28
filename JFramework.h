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
		explicit ArchitectureNotSetException(const std::string& typeName)
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
	template <typename T>
	class IQuery;
	class IUtility;
	template <typename T>
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
			std::lock_guard<std::recursive_mutex> lock(mMutex);
			mSubscribers[eventType].push_back(handler);
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
				std::lock_guard<std::recursive_mutex> lock(mMutex);
				auto it = mSubscribers.find(typeid(*event));
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
				catch (const std::exception& e)
				{
					std::cerr << "[EventBus] Exception in event handler: " << e.what() << std::endl;
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
			std::lock_guard<std::recursive_mutex> lock(mMutex);
			auto it = mSubscribers.find(eventType);
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
			std::lock_guard<std::recursive_mutex> lock(mMutex);
			mSubscribers.clear();
		}

	private:
		std::recursive_mutex mMutex;                                                     ///< Thread safety mutex
		std::unordered_map<std::type_index, std::vector<ICanHandleEvent*>> mSubscribers; ///< Event subscriber map
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
		 * @tparam T System type, must inherit from ISystem
		 * @param system System instance
		 */
		template <typename T>
		void RegisterSystem(std::shared_ptr<T> system)
		{
			if (!system)
			{
				throw std::invalid_argument("ISystem cannot be null");
			}
			static_assert(std::is_base_of_v<ISystem, T>,
				"T must inherit from ISystem");
			RegisterSystem(typeid(T), std::static_pointer_cast<ISystem>(system));
		}

		/**
		 * @brief Template method: Register a model component
		 * @tparam T Model type, must inherit from IModel
		 * @param model Model instance
		 */
		template <typename T>
		void RegisterModel(std::shared_ptr<T> model)
		{
			if (!model)
			{
				throw std::invalid_argument("IModel cannot be null");
			}
			static_assert(std::is_base_of_v<IModel, T>,
				"T must inherit from IModel");
			RegisterModel(typeid(T), std::static_pointer_cast<IModel>(model));
		}

		/**
		 * @brief Template method: Register a utility component
		 * @tparam T Utility type, must inherit from IUtility
		 * @param utility Utility instance
		 */
		template <typename T>
		void RegisterUtility(std::shared_ptr<T> utility)
		{
			if (!utility)
			{
				throw std::invalid_argument("IUtility cannot be null");
			}
			static_assert(std::is_base_of_v<IUtility, T>,
				"T must inherit from IUtility");
			RegisterUtility(typeid(T), std::static_pointer_cast<IUtility>(utility));
		}

		/**
		 * @brief Template method: Get a system component
		 * @tparam T System type
		 * @return System instance
		 * @throws ComponentNotRegisteredException if component is not registered
		 */
		template <typename T>
		std::shared_ptr<T> GetSystem()
		{
			auto system = GetSystem(typeid(T));
			if (!system)
			{
				throw ComponentNotRegisteredException(typeid(T).name());
			}
			return std::dynamic_pointer_cast<T>(system);
		}

		/**
		 * @brief Template method: Get a model component
		 * @tparam T Model type
		 * @return Model instance
		 * @throws ComponentNotRegisteredException if component is not registered
		 */
		template <typename T>
		std::shared_ptr<T> GetModel()
		{
			auto model = GetModel(typeid(T));
			if (!model)
			{
				throw ComponentNotRegisteredException(typeid(T).name());
			}
			return std::dynamic_pointer_cast<T>(model);
		}

		/**
		 * @brief Template method: Get a utility component
		 * @tparam T Utility type
		 * @return Utility instance
		 * @throws ComponentNotRegisteredException if component is not registered
		 */
		template <typename T>
		std::shared_ptr<T> GetUtility()
		{
			auto utility = GetUtility(typeid(T));
			if (!utility)
			{
				throw ComponentNotRegisteredException(typeid(T).name());
			}
			return std::dynamic_pointer_cast<T>(utility);
		}

		/**
		 * @brief Template method: Register an event handler
		 * @tparam T Event type, must inherit from IEvent
		 * @param handler Event handler pointer
		 */
		template <typename T>
		void RegisterEvent(ICanHandleEvent* handler)
		{
			if (!handler)
			{
				throw std::invalid_argument("ICanHandleEvent cannot be null");
			}
			static_assert(std::is_base_of_v<IEvent, T>,
				"T must inherit from IEvent");
			mEventBus->RegisterEvent(typeid(T), handler);
		}

		/**
		 * @brief Template method: Unregister an event handler
		 * @tparam T Event type, must inherit from IEvent
		 * @param handler Event handler pointer
		 */
		template <typename T>
		void UnRegisterEvent(ICanHandleEvent* handler)
		{
			if (!handler)
			{
				throw std::invalid_argument("ICanHandleEvent cannot be null");
			}
			static_assert(std::is_base_of_v<IEvent, T>,
				"T must inherit from IEvent");
			mEventBus->UnRegisterEvent(typeid(T), handler);
		}

		/**
		 * @brief Template method: Send an event
		 * @tparam T Event type, must inherit from IEvent
		 * @tparam Args Constructor argument types
		 * @param args Event constructor arguments
		 */
		template <typename T, typename... Args>
		void SendEvent(Args&&... args)
		{
			static_assert(std::is_base_of_v<IEvent, T>,
				"T must inherit from IEvent");
			this->SendEvent(std::make_shared<T>(std::forward<Args>(args)...));
		}

		/**
		 * @brief Template method: Send a command
		 * @tparam T Command type, must inherit from IJCommand
		 * @tparam Args Constructor argument types
		 * @param args Command constructor arguments
		 */
		template <typename T, typename... Args>
		void SendCommand(Args&&... args)
		{
			static_assert(std::is_base_of_v<IJCommand, T>,
				"T must inherit from ICommand");
			this->SendCommand(std::make_unique<T>(std::forward<Args>(args)...));
		}

		/**
		 * @brief Template method: Send a query and return the result
		 * @tparam T Query type
		 * @param query Query instance
		 * @return Query result
		 */
		template <typename T>
		auto SendQuery(std::unique_ptr<T> query) -> decltype(query->Do())
		{
			static_assert(std::is_base_of_v<IQuery<decltype(query->Do())>, T>,
				"T must inherit from IQuery");

			if (!query)
			{
				throw std::invalid_argument("Query cannot be null");
			}
			query->SetArchitecture(GetSharedFromThis());
			return query->Do();
		}

		/**
		 * @brief Template method: Create and send a query
		 * @tparam T Query type
		 * @tparam Args Constructor argument types
		 * @param args Query constructor arguments
		 * @return Query result
		 */
		template <typename T, typename... Args>
		auto SendQuery(Args&&... args) -> decltype(std::declval<T>().Do())
		{
			static_assert(
				std::is_base_of_v<IQuery<decltype(std::declval<T>().Do())>, T>,
				"T must inherit from IQuery");

			auto query = std::make_unique<T>(std::forward<Args>(args)...);
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
			std::lock_guard<std::recursive_mutex> lock(mMutex);
			mUnRegisters.push_back(std::move(unRegister));
		}

		/**
		 * @brief Execute all unregister operations
		 */
		void UnRegister()
		{
			std::lock_guard<std::recursive_mutex> lock(mMutex);
			for (auto& unRegister : mUnRegisters)
			{
				unRegister->UnRegister();
			}
			mUnRegisters.clear();
		}

	protected:
		std::recursive_mutex mMutex;                                   ///< Thread safety mutex
		std::vector<std::shared_ptr<IUnRegister>> mUnRegisters; ///< List of unregister objects
	};

	/**
	 * @brief Bindable property unregister handler
	 *
	 * Manages observer unregistration for bindable properties
	 * @tparam T Property value type
	 */
	template <typename T>
	class BindablePropertyUnRegister
		: public IUnRegister,
		public std::enable_shared_from_this<BindablePropertyUnRegister<T>>
	{
	public:
		/**
		 * @brief Constructor
		 * @param id Observer ID
		 * @param property Associated bindable property
		 * @param callback Value change callback function
		 */
		BindablePropertyUnRegister(int id,
			BindableProperty<T>* property,
			std::function<void(T)> callback)
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
		void SetProperty(BindableProperty<T>* property)
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
		void Invoke(T value)
		{
			if (mCallback)
			{
				mCallback(std::move(value));
			}
		}

	protected:
		int mId;  ///< Observer ID

	private:
		BindableProperty<T>* mProperty;    ///< Associated bindable property
		std::function<void(T)> mCallback;  ///< Value change callback function
	};

	/**
	 * @brief Bindable property template class
	 *
	 * Observable property implementing the observer pattern, supports value change notifications
	 * @tparam T Property value type
	 */
	template <typename T>
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
			std::lock_guard<std::recursive_mutex> lock(other.mMutex);
			mValue = std::move(other.mValue);
			mObservers = std::move(other.mObservers);
			mNextId = other.mNextId;
			for (auto& pair : mObservers)
			{
				if (pair.second)
					pair.second->SetProperty(this);
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
				std::lock_guard<std::recursive_mutex> lock1(mMutex);
				std::lock_guard<std::recursive_mutex> lock2(other.mMutex);
				mValue = std::move(other.mValue);
				mObservers = std::move(other.mObservers);
				mNextId = other.mNextId;
				for (auto& pair : mObservers)
				{
					if (pair.second)
						pair.second->SetProperty(this);
				}
				other.mNextId = 0;
			}
			return *this;
		}

		/**
		 * @brief Constructor with initial value (copy)
		 * @param value Initial value
		 */
		explicit BindableProperty(const T& value)
			: mValue(value)
		{
		}

		/**
		 * @brief Constructor with initial value (move)
		 * @param value Initial value
		 */
		explicit BindableProperty(T&& value)
			: mValue(std::move(value))
		{
		}

		/**
		 * @brief Destructor, clears all observers
		 */
		~BindableProperty()
		{
			std::lock_guard<std::recursive_mutex> lock(mMutex);
			mObservers.clear();
		}

		/**
		 * @brief Get current value
		 * @return Current value
		 */
		const T& GetValue() const { return mValue; }

		/**
		 * @brief Set new value and notify observers
		 * @param newValue New value
		 */
		void SetValue(const T& newValue)
		{
			std::lock_guard<std::recursive_mutex> lock(mMutex);
			if (mValue == newValue)
				return;
			mValue = newValue;
			for (auto& pair : mObservers)
			{
				try
				{
					pair.second->Invoke(mValue);
				}
				catch (const std::exception& e)
				{
					std::cerr << "[BindableProperty] Exception in observer callback: " << e.what() << std::endl;
				}
			}
		}

		/**
		 * @brief Set new value without triggering notification
		 * @param newValue New value
		 */
		void SetValueWithoutEvent(const T& newValue)
		{
			std::lock_guard<std::recursive_mutex> lock(mMutex);
			mValue = newValue;
		}

		/**
		 * @brief Register observer and send initial value notification
		 * @param onValueChanged Value change callback function
		 * @return Unregister object
		 */
		std::shared_ptr<BindablePropertyUnRegister<T>> RegisterWithInitValue(
			std::function<void(const T&)> onValueChanged)
		{
			onValueChanged(mValue);
			return Register(std::move(onValueChanged));
		}

		/**
		 * @brief Register observer
		 * @param onValueChanged Value change callback function
		 * @return Unregister object
		 */
		std::shared_ptr<BindablePropertyUnRegister<T>> Register(
			std::function<void(const T&)> onValueChanged)
		{
			std::lock_guard<std::recursive_mutex> lock(mMutex);
			auto unRegister = std::make_shared<BindablePropertyUnRegister<T>>(
				mNextId++, this, std::move(onValueChanged));
			mObservers[unRegister->GetId()] = unRegister;
			return unRegister;
		}

		/**
		 * @brief Unregister observer
		 * @param id Observer ID
		 */
		void UnRegister(int id)
		{
			std::lock_guard<std::recursive_mutex> lock(mMutex);
			mObservers.erase(id);
		}

		/// Type conversion operator
		operator T() const { return mValue; }

		/**
		 * @brief Copy assignment operator
		 * @param newValue New value
		 * @return Reference to this object
		 */
		BindableProperty<T>& operator=(const T& newValue)
		{
			SetValue(newValue);
			return *this;
		}

		/**
		 * @brief Move assignment operator
		 * @param newValue New value
		 * @return Reference to this object
		 */
		BindableProperty<T>& operator=(T&& newValue)
		{
			SetValue(std::move(newValue));
			return *this;
		}

	private:
		std::recursive_mutex mMutex;       ///< Thread safety mutex
		int mNextId = 0;         ///< Next observer ID
		T mValue;              ///< Property value
		std::unordered_map<int, std::shared_ptr<BindablePropertyUnRegister<T>>> mObservers; ///< Observer list
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
		 * @tparam T Model type
		 * @return Model instance
		 */
		template <typename T>
		std::shared_ptr<T> GetModel()
		{
			static_assert(std::is_base_of_v<IModel, T>,
				"T must inherit from IModel");

			auto arch = GetArchitecture().lock();
			if (!arch)
			{
				throw ArchitectureNotSetException(typeid(T).name());
			}

			auto model = arch->GetModel<T>();
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
		 * @tparam T System type
		 * @return System instance
		 */
		template <typename T>
		std::shared_ptr<T> GetSystem()
		{
			static_assert(std::is_base_of_v<ISystem, T>,
				"T must inherit from ISystem");

			auto arch = GetArchitecture().lock();
			if (!arch)
			{
				throw ArchitectureNotSetException(typeid(T).name());
			}

			auto system = arch->GetSystem<T>();
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
		 * @tparam T Command type
		 * @tparam Args Constructor argument types
		 * @param args Command constructor arguments
		 */
		template <typename T, typename... Args>
		void SendCommand(Args&&... args)
		{
			static_assert(std::is_base_of_v<IJCommand, T>,
				"T must inherit from IJCommand");

			auto arch = GetArchitecture().lock();
			if (!arch)
			{
				throw ArchitectureNotSetException(typeid(T).name());
			}
			arch->SendCommand<T>(std::forward<Args>(args)...);
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
		 * @tparam T Query type
		 * @tparam Args Constructor argument types
		 * @param args Query constructor arguments
		 * @return Query result
		 */
		template <typename T, typename... Args>
		auto SendQuery(Args&&... args) -> decltype(std::declval<T>().Do())
		{
			auto arch = GetArchitecture().lock();
			if (!arch)
			{
				throw ArchitectureNotSetException(typeid(T).name());
			}

			auto result = arch->SendQuery<T>(std::forward<Args>(args)...);
			return result;
		}

		/**
		 * @brief Send a query
		 * @tparam T Query type
		 * @param query Query instance
		 * @return Query result
		 */
		template <typename T>
		auto SendQuery(std::unique_ptr<T> query)
			-> decltype(std::declval<T>().Do())
		{
			auto arch = GetArchitecture().lock();
			if (!arch)
			{
				throw ArchitectureNotSetException(typeid(T).name());
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
		 * @tparam T Utility type
		 * @return Utility instance
		 */
		template <typename T>
		std::shared_ptr<T> GetUtility()
		{
			static_assert(std::is_base_of_v<IUtility, T>,
				"T must inherit from IUtility");

			auto arch = GetArchitecture().lock();
			if (!arch)
			{
				throw ArchitectureNotSetException(typeid(T).name());
			}

			auto utility = arch->GetUtility<T>();
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
		 * @tparam T Event type
		 * @tparam Args Constructor argument types
		 * @param args Event constructor arguments
		 */
		template <typename T, typename... Args>
		void SendEvent(Args&&... args)
		{
			static_assert(std::is_base_of_v<IEvent, T>,
				"T must inherit from IEvent");

			auto arch = GetArchitecture().lock();
			if (!arch)
			{
				throw ArchitectureNotSetException(typeid(T).name());
			}
			arch->SendEvent<T>(std::forward<Args>(args)...);
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
		 * @tparam T Event type
		 * @param handler Event handler
		 */
		template <typename T>
		void RegisterEvent(ICanHandleEvent* handler)
		{
			static_assert(std::is_base_of_v<IEvent, T>,
				"T must inherit from IEvent");

			auto arch = GetArchitecture().lock();
			if (!arch)
			{
				throw ArchitectureNotSetException(typeid(T).name());
			}

			arch->RegisterEvent<T>(handler);
		}

		/**
		 * @brief Unregister an event handler
		 * @tparam T Event type
		 * @param handler Event handler
		 */
		template <typename T>
		void UnRegisterEvent(ICanHandleEvent* handler)
		{
			static_assert(std::is_base_of_v<IEvent, T>,
				"T must inherit from IEvent");

			auto arch = GetArchitecture().lock();
			if (!arch)
			{
				throw ArchitectureNotSetException(typeid(T).name());
			}

			arch->UnRegisterEvent<T>(handler);
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
	 * @tparam T Query result type
	 */
	template <typename T>
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
		virtual T Do() = 0;
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
		 * @tparam T Component type
		 * @tparam TBase Component base type
		 * @param typeId Type index
		 * @param component Component instance
		 */
		template <typename T, typename TBase>
		void Register(std::type_index typeId, std::shared_ptr<TBase> component)
		{
			static_assert(std::is_base_of_v<TBase, T>, "T must inherit from TBase");
			auto& container = GetContainer(ContainerTypeTag<TBase> {});
			std::lock_guard<std::recursive_mutex> lock(GetMutex(MutexTypeTag<TBase> {}));

			if (container.find(typeId) != container.end())
				throw ComponentAlreadyRegisteredException(typeId.name());

			container[typeId] = std::static_pointer_cast<TBase>(component);
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
			std::lock_guard<std::recursive_mutex> lock(GetMutex(MutexTypeTag<TBase> {}));
			auto it = container.find(typeId);
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
			std::lock_guard<std::recursive_mutex> lock(GetMutex(MutexTypeTag<TBase> {}));
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
			std::lock_guard<std::recursive_mutex> lock1(mModelMutex);
			std::lock_guard<std::recursive_mutex> lock2(mSystemMutex);
			std::lock_guard<std::recursive_mutex> lock3(mUtilityMutex);
			mModels.clear();
			mSystems.clear();
			mUtilities.clear();
		}

	private:
		/// Container type tag
		template <typename>
		struct ContainerTypeTag {};

		auto& GetContainer(ContainerTypeTag<IModel>) { return mModels; }
		auto& GetContainer(ContainerTypeTag<ISystem>) { return mSystems; }
		auto& GetContainer(ContainerTypeTag<IUtility>) { return mUtilities; }

		/// Mutex type tag
		template <typename>
		struct MutexTypeTag {};

		auto& GetMutex(MutexTypeTag<IModel>) { return mModelMutex; }
		auto& GetMutex(MutexTypeTag<ISystem>) { return mSystemMutex; }
		auto& GetMutex(MutexTypeTag<IUtility>) { return mUtilityMutex; }

		std::unordered_map<std::type_index, std::shared_ptr<IModel>> mModels;     ///< Model container
		std::unordered_map<std::type_index, std::shared_ptr<ISystem>> mSystems;   ///< System container
		std::unordered_map<std::type_index, std::shared_ptr<IUtility>> mUtilities; ///< Utility container

		std::recursive_mutex mModelMutex;   ///< Model container mutex
		std::recursive_mutex mSystemMutex;  ///< System container mutex
		std::recursive_mutex mUtilityMutex; ///< Utility container mutex
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

			for (auto& system : mContainer->GetAll<ISystem>())
			{
				UnInitializeComponent(system);
			}

			for (auto& model : mContainer->GetAll<IModel>())
			{
				UnInitializeComponent(model);
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
		 * @tparam T Component type
		 * @param component Component instance
		 */
		template <typename T>
		void InitializeComponent(std::shared_ptr<T> component)
		{
			static_assert(std::is_base_of_v<ICanInit, T>,
				"Component must implement ICanInit");

			if (!component->IsInitialized())
			{
				component->Init();
				component->SetInitialized(true);
			}
		}

		/**
		 * @brief Deinitialize a component
		 * @tparam T Component type
		 * @param component Component instance
		 */
		template <typename T>
		void UnInitializeComponent(std::shared_ptr<T> component)
		{
			static_assert(std::is_base_of_v<ICanInit, T>,
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
	public:
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
	 * @tparam T Query result type
	 */
	template <typename T>
	class AbstractQuery : public IQuery<T>
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
		T Do() final { return OnDo(); }

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
		virtual T OnDo() = 0;
	};
}; // namespace JFramework

#endif // !_JFRAMEWORK_
