#include <emCore/task/taskmaster.hpp>
#include <emCore/task/task_config.hpp>

using namespace emCore;

// Example task functions
void task_led_blink(void* params) {
    // Blink LED logic here
    // digitalWrite(LED_PIN, !digitalRead(LED_PIN));
}

void task_telemetry(void* params) {
    // Send telemetry data
    // sendTelemetry();
}

void task_sensor_read(void* params) {
    // Read sensor data
    // readSensors();
}

// Task configuration table - all tasks defined at compile time
constexpr task_config task_table[] = {
    // Function,          Name,              Priority,          Period(ms), Params, Enabled
    {task_led_blink,      "LED_Blink",       priority::low,     500,        nullptr, true},
    {task_telemetry,      "Telemetry",       priority::normal,  1000,       nullptr, true},
    {task_sensor_read,    "Sensor_Read",     priority::high,    100,        nullptr, true},
};

void setup() {
    // Initialize the taskmaster singleton
    auto& tm = taskmaster::instance();
    
    auto init_result = tm.initialize();
    if (init_result.is_error()) {
        // Handle initialization error
        return;
    }
    
    // Create all tasks from configuration table
    auto create_result = tm.create_all_tasks(task_table);
    if (create_result.is_error()) {
        // Handle task creation error
        return;
    }
}

void loop() {
    // Run the task scheduler - call this in your main loop
    taskmaster::instance().run();
    
    // Optional: Add a small delay to prevent CPU hogging
    // delay(1);
}

// Alternative: Manual task creation
void manual_task_creation_example() {
    auto& tm = taskmaster::instance();
    
    // Create individual task
    task_config cfg(
        task_led_blink,
        "Manual_LED",
        priority::normal,
        1000,  // 1 second period
        nullptr,
        true
    );
    
    auto result = tm.create_task(cfg);
    if (result.is_ok()) {
        task_id_t task_id = result.value();
        
        // Can suspend/resume tasks
        tm.suspend_task(task_id);
        tm.resume_task(task_id);
        
        // Get task information
        auto info_result = tm.get_task_info(task_id);
        if (info_result.is_ok()) {
            const auto* tcb = info_result.value();
            // Access task control block data
            // tcb->run_count, tcb->execution_time, etc.
        }
    }
}
