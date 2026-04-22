// JFramework.cpp : Example application demonstrating JFramework usage.

#include "JFramework.h"
#include <iostream>
using namespace JFramework;

void BindablePropertyExample()
{
	// 1. Declare a bindable property
	BindableProperty<int> counter(0);

	// 2. Register an observer to listen for value changes
	auto unreg = counter.Register([](const int& value)
		{
			std::cout << "Counter changed to: " << value << std::endl;
		});

	// 3. Modify the value to trigger notifications
	counter.SetValue(1); // Output: Counter changed to: 1
	counter = 2;         // Output: Counter changed to: 2

	// 4. Unregister the observer (no more notifications)
	unreg->Unregister();
	counter.SetValue(3); // No output

	// 5. Register with initial value notification
	auto unreg2 = counter.RegisterWithInitValue([](const int& value)
		{
			std::cout << "Init observer, value: " << value << std::endl;
		});
	// Output: Init observer, value: 3

	// 6. Assignment triggers notification again
	counter = 10; // Output: Init observer, value: 10

	BindableProperty<int> autoCounter(100);
	// 7. Auto-unregister observer (optional, using UnregisterTrigger)
	{
		UnregisterTrigger trigger;

		auto autoUnreg = autoCounter.Register([](const int& value)
			{
				std::cout << "Auto observer: " << value << std::endl;
			});
		// Bind to trigger; auto-unregisters when trigger destructs
		autoUnreg->UnregisterWhenObjectDestroyed(&trigger);

		autoCounter = 101; // Output: Auto observer: 101

		// trigger leaves scope, autoUnreg is automatically unregistered
	}
	// Assigning autoCounter again produces no output
}

// 1. Define an event
class MyEvent : public IEvent
{
public:
	std::string msg;
	MyEvent(const std::string& m) : msg(m) {}
};

// 2. Define a Model
class CounterModel : public AbstractModel
{
public:
	int value = 0;
protected:
	void OnInit() override { value = 0; }
	void OnDeinit() override {}
};

// 1. Define a Utility
class LoggerUtility : public IUtility
{
public:
	void Log(const std::string& msg)
	{
		std::cout << "[Logger] " << msg << std::endl;
	}
};

// 2. Define a Model that uses a Utility
class MyModel : public AbstractModel
{
protected:
	void OnInit() override
	{
		auto logger = GetUtility<LoggerUtility>();
		logger->Log("MyModel initialized");
	}
	void OnDeinit() override {}
};

// 3. Define a System that listens for events
class PrintSystem : public AbstractSystem
{
protected:
	void OnInit() override
	{
		RegisterEvent<MyEvent>(this);
	}
	void OnDeinit() override
	{
		UnregisterEvent<MyEvent>(this);
	}
	void OnEvent(std::shared_ptr<IEvent> event) override
	{
		auto e = std::dynamic_pointer_cast<MyEvent>(event);
		if (e)
		{
			std::cout << "PrintSystem received event: " << e->msg << std::endl;
		}
	}
};

// 4. Define a Command
class AddCommand : public AbstractCommand
{
	int delta;
public:
	AddCommand(int d) : delta(d) {}
protected:
	void OnExecute() override
	{
		auto model = GetModel<CounterModel>();
		model->value += delta;
		SendEvent<MyEvent>("Counter increased, current value: " + std::to_string(model->value));
	}
};

// 1. Define a Model for query testing
class TestQueryCounterModel : public AbstractModel
{
public:
	int value = 42;
protected:
	void OnInit() override { value = 42; }
	void OnDeinit() override {}
};

// 3. Define a Command that uses a Utility
class PrintCommand : public AbstractCommand
{
	std::string mMsg;
public:
	PrintCommand(const std::string& msg) : mMsg(msg) {}
protected:
	void OnExecute() override
	{
		auto logger = GetUtility<LoggerUtility>();
		logger->Log("PrintCommand executing: " + mMsg);
	}
};

// 2. Define a Query to retrieve the CounterModel value
class GetCounterValueQuery : public AbstractQuery<int>
{
protected:
	int OnDo() override
	{
		// Get Model via base class interface
		auto model = GetModel<TestQueryCounterModel>();
		return model->value;
	}
};

// 5. Define the architecture implementation
class MyAppArchitecture : public Architecture
{
protected:
	void Init() override
	{
		RegisterUtility(std::make_shared<LoggerUtility>());

		RegisterModel(std::make_shared<MyModel>());
		RegisterModel(std::make_shared<CounterModel>());
		RegisterModel(std::make_shared<TestQueryCounterModel>());

		RegisterSystem(std::make_shared<PrintSystem>());
	}
	void OnDeinit() override {}
};

int ArchitectureExample()
{
	// Create architecture instance
	auto arch = std::make_shared<MyAppArchitecture>();
	arch->InitArchitecture();

	// Send commands
	arch->SendCommand<AddCommand>(5);
	arch->SendCommand<AddCommand>(3);

	// Get Model
	auto model = arch->GetModel<CounterModel>();
	std::cout << "Final counter value: " << model->value << std::endl;

	// Send a command that internally uses a Utility
	arch->SendCommand<PrintCommand>("Hello Utility!");

	// Send a Query through the architecture to get CounterModel value
	int result = arch->SendQuery<GetCounterValueQuery>();
	std::cout << "CounterModel value: " << result << std::endl; // Output: 42

	arch->Deinit();
	return 0;
}

int main(int argc, char** argv)
{
	BindablePropertyExample();

	ArchitectureExample();

	return 0;
}
