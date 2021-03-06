/*
 * Copyright (c) 2013-2016, ARM Limited, All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


/*
 * Tests is to measure the accuracy of Ticker over a period of time
 *
 *
 * 1) DUT would start to update callback_trigger_count every milli sec, in 2x callback we use 2 tickers
 *    to update the count alternatively.
 * 2) Host would query what is current count base_time, Device responds by the callback_trigger_count
 * 3) Host after waiting for measurement stretch. It will query for device time again final_time.
 * 4) Host computes the drift considering base_time, final_time, transport delay and measurement stretch
 * 5) Finally host send the results back to device pass/fail based on tolerance.
 * 6) More details on tests can be found in timing_drift_auto.py
 */

#include "mbed.h"
#include "greentea-client/test_env.h"
#include "utest/utest.h"
#include "unity/unity.h"

using namespace utest::v1;

#define ONE_MILLI_SEC 1000
volatile uint32_t callback_trigger_count = 0;
static const int test_timeout = 240;
static const int total_ticks = 10;

DigitalOut led1(LED1);
DigitalOut led2(LED2);

Ticker *ticker1;
Ticker *ticker2;

volatile int ticker_count = 0;
volatile bool print_tick = false;

void ticker_callback_1_switch_to_2(void);
void ticker_callback_2_switch_to_1(void);

void ticker_callback_0(void) {
    ++callback_trigger_count;
}

void ticker_callback_1_led(void) {
    led1 = !led1;
}

void ticker_callback_2_led(void) {
    led2 = !led2;
}

void ticker_callback_1_switch_to_2(void) {
    ++callback_trigger_count;
    ticker1->detach();
    ticker1->attach_us(ticker_callback_2_switch_to_1, ONE_MILLI_SEC);
    ticker_callback_1_led();
}

void ticker_callback_2_switch_to_1(void) {
    ++callback_trigger_count;
    ticker2->detach();
    ticker2->attach_us(ticker_callback_1_switch_to_2, ONE_MILLI_SEC);
    ticker_callback_2_led();
}

void wait_and_print() {
    while (ticker_count <= total_ticks) {
        if (print_tick) {
            print_tick = false;
            greentea_send_kv("tick", ticker_count++);
        }
    }
}

void test_case_1x_ticker() {

    char _key[11] = { };
    char _value[128] = { };
    uint8_t results_size = 0;
    int expected_key = 1;

    greentea_send_kv("timing_drift_check_start", 0);
    ticker1->attach_us(&ticker_callback_0, ONE_MILLI_SEC);

    // wait for 1st signal from host
    do {
        greentea_parse_kv(_key, _value, sizeof(_key), sizeof(_value));
        expected_key = strcmp(_key, "base_time");
    } while (expected_key);
    greentea_send_kv(_key, callback_trigger_count * ONE_MILLI_SEC);

    // wait for 2nd signal from host
    greentea_parse_kv(_key, _value, sizeof(_key), sizeof(_value));
    greentea_send_kv(_key, callback_trigger_count * ONE_MILLI_SEC);

    //get the results from host
    greentea_parse_kv(_key, _value, sizeof(_key), sizeof(_value));

    TEST_ASSERT_EQUAL_STRING_MESSAGE("pass", _key,"Host side script reported a fail...");

}

void test_case_2x_callbacks() {
    char _key[11] = { };
    char _value[128] = { };
    uint8_t results_size = 0;
    int expected_key =  1;

    led1 = 0;
    led2 = 0;
    callback_trigger_count = 0;

    greentea_send_kv("timing_drift_check_start", 0);
    ticker1->attach_us(ticker_callback_1_switch_to_2, ONE_MILLI_SEC);

    // wait for 1st signal from host
    do {
        greentea_parse_kv(_key, _value, sizeof(_key), sizeof(_value));
        expected_key = strcmp(_key, "base_time");
    } while (expected_key);
    greentea_send_kv(_key, callback_trigger_count * ONE_MILLI_SEC);

    // wait for 2nd signal from host
    greentea_parse_kv(_key, _value, sizeof(_key), sizeof(_value));
    greentea_send_kv(_key, callback_trigger_count * ONE_MILLI_SEC);

    //get the results from host
    greentea_parse_kv(_key, _value, sizeof(_key), sizeof(_value));

    TEST_ASSERT_EQUAL_STRING_MESSAGE("pass", _key,"Host side script reported a fail...");

}

utest::v1::status_t one_ticker_case_setup_handler_t(const Case *const source, const size_t index_of_case) {
    ticker1 = new Ticker();
    return greentea_case_setup_handler(source, index_of_case);
}

utest::v1::status_t two_ticker_case_setup_handler_t(const Case *const source, const size_t index_of_case) {
    ticker1 = new Ticker();
    ticker2 = new Ticker();
    return greentea_case_setup_handler(source, index_of_case);
}

utest::v1::status_t one_ticker_case_teardown_handler_t(const Case *const source, const size_t passed, const size_t failed, const failure_t reason) {
    delete ticker1;
    return greentea_case_teardown_handler(source, passed, failed, reason);
}

utest::v1::status_t two_ticker_case_teardown_handler_t(const Case *const source, const size_t passed, const size_t failed, const failure_t reason) {
    delete ticker1;
    delete ticker2;
    return greentea_case_teardown_handler(source, passed, failed, reason);
}

// Test cases
Case cases[] = {
    Case("Timers: 1x ticker", one_ticker_case_setup_handler_t,test_case_1x_ticker, one_ticker_case_teardown_handler_t),
    Case("Timers: 2x callbacks", two_ticker_case_setup_handler_t,test_case_2x_callbacks, two_ticker_case_teardown_handler_t),
};

utest::v1::status_t greentea_test_setup(const size_t number_of_cases) {
    GREENTEA_SETUP(test_timeout, "timing_drift_auto");
    return greentea_test_setup_handler(number_of_cases);
}

Specification specification(greentea_test_setup, cases, greentea_test_teardown_handler);

int main() {
    Harness::run(specification);
}
