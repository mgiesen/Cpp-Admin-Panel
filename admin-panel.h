#pragma once

// HTTP server library from: https://github.com/yhirose/cpp-httplib
#include <httplib.h>

// Standard Includes
#include <map>
#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <thread>

namespace ADMIN_PANEL
{
    // Typedef for function pointers
    typedef void (*FnPtr)(httplib::Response& res);

    // Structure to hold details about each admin function
    struct admin_function
    {
        std::string function_group = "";
        std::string function_name = "";
        bool is_pinned = false;
        FnPtr function;
    };

    class PANEL
    {
    private:

        // HTML template for admin panel
        std::string admin_panel_html_template = R"raw(
            <!DOCTYPE html>
            <html>

            <head>
                <meta charset="UTF-8">
                <meta name="viewport" content="width=device-width, initial-scale=1.0">
                <title>#REPLACE_PAGE_TITLE</title>
                <style>

                    html {
                        background: rgb(69, 113, 148);
                        margin: 0;
                        padding: 0;
                        font-family: monospace;
                        color: white;
                    }

                    body {
                        margin: 0;
                        padding: 20px;
                    }

                    h1 {
                        margin-top: 0;
                        font-family: Arial, Helvetica, sans-serif;
                    }

                    h2 {
                        margin: 0;
                    }

                    p {
                        margin-bottom: 0;
                    }

                    textarea {
                        display: block;
                        width: 100%;
                        min-height: 200px;
                        resize: none;
                        -webkit-box-sizing: border-box;
                        -moz-box-sizing: border-box;
                        box-sizing: border-box;
                        background-color: rgba(255, 255, 255, 0.5);
                        border: 0;
                        border-radius: 5px;
                        padding: 8px;
                    }

                    button {
                        height: 35px;
                        padding-right: 20px;
                        padding-left: 20px;
                        white-space: nowrap;
                        color: black;
                    }

                    select {
                        padding-left: 5px;
                        width: 100%;
                        height: 35px;
                        border-radius: 5px;
                        color: black;
                    }

                    .container {
                        box-sizing: border-box;
                        width: 100%;
                        background-color: rgba(255, 255, 255, 0.5);
                        border-radius: 5px;
                        padding: 10px;
                        margin-top: 5px;
                    }

                    .actions {
                        display: flex;
                        flex-direction: column;
                        gap: 10px;
                    }

                    .pinned_actions {
                        display: flex;
                        flex-direction: column;
                        gap: 2px;
                        color: black;
                    }

                    .action_bar_pinned_actions button {
                        width: 100%;
                    }

                </style>
            </head>

            <body>

                <h1>#REPLACE_PAGE_HEADER</h1>

                <p>Pinned Requests</p>
                <div class="pinned_actions container">#REPLACE_PINNED_BUTTONS</div>
                <p>Requests Groups</p>
                <div class="actions container">#REPLACE_DROPDOWNS</div>
                <p>Request Response</p>
                <div class="container">
                    <textarea spellcheck="false" id="response_txt" placeholder="Wait for Request..."></textarea>
                </div>

                <script>

                    //CONSTANTS
                    const HOSTNAME = `http://${window.location.hostname}`;
                    const REST_PORT = window.location.port === "" ? "80" : window.location.port;
                    const REST_URL = `${HOSTNAME}:${REST_PORT}`;

                    //FUNCTIONS
                    function call_function(target_dropdown) 
                    {
                        const payload = document.getElementById(target_dropdown).value;
                        const url = `${REST_URL}/call_function`;

                        document.getElementById("response_txt").value = "Please wait. Request is pending...";
                        send_request("POST", url, payload);
                    }

                    function call_pinned_function(payload) 
                    {
                        const url = `${REST_URL}/call_function`;
                        send_request("POST", url, payload);
                    }

                    async function send_request(request_method, url, payload) 
                    {
                        try 
                        {
                            const response = await fetch(url, {
                                method: request_method,
                                headers: { "Content-Type": "text/plain" },
                                body: payload
                            });

                            let output;
                            if (response.status === 200) 
                            {
                                const text = await response.text();

                                if (text === "") 
                                {
                                    output = "Request was successful";
                                } 
                                else 
                                {
                                    try 
                                    {
                                        const json_data = JSON.parse(text);
                                        output = JSON.stringify(json_data, null, 4);
                                    } 
                                    catch (error) 
                                    {
                                        output = text;
                                    }
                                }
                            } 
                            else 
                            {
                                output = "Request failed";
                            }

                            document.getElementById("response_txt").value = output;
                        } 
                        catch (error) 
                        {
                            document.getElementById("response_txt").value = error;
                        }
                    }

                    // DOM EVENTS
                    window.addEventListener('DOMContentLoaded', () => {
                        const pinned_actions_div = document.querySelector('.pinned_actions');
                        if (!pinned_actions_div.innerHTML.trim()) {
                            pinned_actions_div.innerHTML = 'No pinned functions';
                        }
                    });

                </script>

            </body>

            </html>
        )raw";

        // User-provided parameters
        int m_port;
        std::map<std::string, ADMIN_PANEL::admin_function> m_admin_functions = {};
        std::string m_panel_title = "";
        std::string m_browser_title = "";

        // Class variables
        httplib::Server m_rest;
        std::thread m_server_thread;

        bool replace(std::string& str, const std::string& from, const std::string& to)
        {
            size_t start_pos = str.find(from);
            if (start_pos == std::string::npos)
                return false;
            str.replace(start_pos, from.length(), to);
            return true;
        }

        void parse_map_to_html(std::string& dropdown_data, std::string& pinned_data)
        {
            int group_id = 1;
            std::string current_group = "";

            // The map must be sorted so that the admin_function grouping can be assigned.
            std::vector<std::pair<std::string, admin_function>> sorted_admin_functions(m_admin_functions.begin(), m_admin_functions.end());

            std::sort(sorted_admin_functions.begin(), sorted_admin_functions.end(),[](const auto& a, const auto& b) {
                    return a.second.function_group < b.second.function_group;
            });

            for (const auto it : sorted_admin_functions)
            {
                if (it.second.is_pinned)
                {
                    pinned_data += "<button onclick=\"call_pinned_function('" + it.first + "')\">" + it.second.function_name + "</button>\n";
                }
                else
                {
                    const std::string iterator_function_group = it.second.function_group;

                    if (iterator_function_group != current_group)
                    {
                        if (current_group != "")
                        {
                            dropdown_data += "</select>";
                            dropdown_data += "<button onclick=\"call_function('" + std::to_string(group_id) + "')\">Request senden</ button>";
                            dropdown_data += "</div>";
                        }

                        current_group = iterator_function_group;
                        group_id++;

                        dropdown_data += "<h2 style=\"display: flex; gap : 10px;\">" + iterator_function_group + "</h2>";
                        dropdown_data += "<div style=\"display: flex; gap : 10px;\">";
                        dropdown_data += "<select id='" + std::to_string(group_id) + "'>"; 
                    }

                    dropdown_data += "<option value =\"" + it.first + "\">" + it.second.function_name + "</option>\n";
                }
            }

            if (dropdown_data.empty() == false)
            {
                dropdown_data += "</select>";
                dropdown_data += "<button onclick=\"call_function('" + std::to_string(group_id) + "')\">Request senden</ button>";
                dropdown_data += "</div>";
            }

        }

        std::string build_admin_panel()
        {
            replace(admin_panel_html_template, "#REPLACE_PAGE_TITLE", m_panel_title);
            replace(admin_panel_html_template, "#REPLACE_PAGE_HEADER", m_browser_title);

            std::string dropdown_data = "";
            std::string pinned_data = "";

            parse_map_to_html(dropdown_data, pinned_data);

            replace(admin_panel_html_template, "#REPLACE_DROPDOWNS", dropdown_data);
            replace(admin_panel_html_template, "#REPLACE_PINNED_BUTTONS", pinned_data);

            return admin_panel_html_template;
        }

        void stop()
        {
            m_rest.stop();
            if (m_server_thread.joinable()) {
                m_server_thread.join();
            }
        }

        void start_admin_panel(const int port)
        {
            try
            {
                std::string admin_panel_html_template_html = build_admin_panel();

                m_rest.Get("/", [admin_panel_html_template_html](const httplib::Request& req, httplib::Response& res) {
                    res.set_content(admin_panel_html_template_html, "text/html");
                });

                m_rest.Post("/call_function", [this](const httplib::Request& req, httplib::Response& res) { 
                    m_admin_functions[req.body].function(res);
                });

                m_rest.listen("0.0.0.0", port);
            }
            catch (const std::exception& e)
            {
                std::cerr << "[ADMIN PANEL] Your panel " + m_panel_title + " stopped with the error: " << e.what() << std::endl;
                stop();
            }
        }

    public:

        // Constructor of admin panel
        PANEL(
            int port, 
            std::map<std::string, ADMIN_PANEL::admin_function>& admin_functions, 
            std::string panel_title, 
            std::string divergent_browser_title = "")
            {
                m_port = port;
                m_admin_functions = admin_functions;
                m_panel_title = panel_title;
                m_browser_title = divergent_browser_title == "" ? panel_title : divergent_browser_title;

                m_server_thread = std::thread([this]() { start_admin_panel(m_port); });
            }

        // Destructor of admin panel
        ~PANEL()
        {
            stop();
        }
    };

}
