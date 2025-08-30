#include "run/run.hpp"
#include "app.hpp"

FlipMapRun::FlipMapRun(void *appContext) : appContext(appContext), currentView(FlipMapRunViewLogin), inputHeld(false),
                                           lastInput(InputKeyMAX), locationStatus(LocationNotStarted), loginStatus(LoginNotStarted), mapDataStatus(MapDataNotStarted),
                                           registrationStatus(RegistrationNotStarted), shouldDebounce(false), shouldReturnToMenu(false)
{
    char *loginStatusStr = (char *)malloc(32);
    if (loginStatusStr)
    {
        FlipMapApp *app = static_cast<FlipMapApp *>(appContext);
        if (app && app->loadChar("login_status", loginStatusStr, 32))
        {
            if (strcmp(loginStatusStr, "success") == 0)
            {
                loginStatus = LoginSuccess;
                locationStatus = LocationWaiting;
                currentView = FlipMapRunViewLocation;
                userRequest(RequestTypeLocationUpdate);
            }
            else
            {
                loginStatus = LoginNotStarted;
                currentView = FlipMapRunViewLogin;
            }
        }
        free(loginStatusStr);
    }
}

FlipMapRun::~FlipMapRun()
{
    // nothing to do
}

void FlipMapRun::debounceInput()
{
    static uint8_t debounceCounter = 0;
    if (shouldDebounce)
    {
        lastInput = InputKeyMAX;
        debounceCounter++;
        if (debounceCounter < 2)
        {
            return;
        }
        debounceCounter = 0;
        shouldDebounce = false;
        inputHeld = false;
    }
}

void FlipMapRun::drawLocationView(Canvas *canvas)
{
    canvas_set_font(canvas, FontPrimary);
    static bool loadingStarted = false;
    switch (locationStatus)
    {
    case LocationWaiting:
        if (!loadingStarted)
        {
            if (!loading)
            {
                loading = std::make_unique<Loading>(canvas);
            }
            loadingStarted = true;
            if (loading)
            {
                loading->setText("Syncing...");
            }
        }
        if (!this->httpRequestIsFinished())
        {
            if (loading)
            {
                loading->animate();
            }
        }
        else
        {
            if (loading)
            {
                loading->stop();
            }
            loadingStarted = false;
            FlipMapApp *app = static_cast<FlipMapApp *>(appContext);
            if (app->getHttpState() == ISSUE)
            {
                locationStatus = LocationRequestError;
                return;
            }
            char response[256];
            if (app && app->loadChar("location_status", response, sizeof(response)))
            {
                if (strstr(response, "[ERROR]") == NULL)
                {
                    locationStatus = LocationSuccess;
                    currentView = FlipMapRunViewMapData;
                    mapDataStatus = MapDataWaiting;
                    userRequest(RequestTypeMapData);
                }
                else
                {
                    locationStatus = LocationRequestError;
                }
            }
            else
            {
                locationStatus = LocationRequestError;
            }
        }
        break;
    case LocationSuccess:
        canvas_draw_str(canvas, 0, 10, "Location successful!");
        canvas_draw_str(canvas, 0, 20, "Press OK to continue.");
        break;
    case LocationCredentialsMissing:
        canvas_draw_str(canvas, 0, 10, "Missing credentials!");
        canvas_draw_str(canvas, 0, 20, "Please set your username");
        canvas_draw_str(canvas, 0, 30, "and password in the app.");
        break;
    case LocationRequestError:
        canvas_draw_str(canvas, 0, 10, "Location request failed!");
        canvas_draw_str(canvas, 0, 20, "Check your network and");
        canvas_draw_str(canvas, 0, 30, "try again later.");
        break;
    case LocationNotStarted:
        locationStatus = LocationWaiting;
        userRequest(RequestTypeLocationUpdate);
        break;
    default:
        FURI_LOG_E(TAG, "Unknown location status");
        canvas_draw_str(canvas, 0, 10, "Syncing...");
        break;
    }
}

void FlipMapRun::drawLoginView(Canvas *canvas)
{
    canvas_set_font(canvas, FontPrimary);
    static bool loadingStarted = false;
    switch (loginStatus)
    {
    case LoginWaiting:
        if (!loadingStarted)
        {
            if (!loading)
            {
                loading = std::make_unique<Loading>(canvas);
            }
            loadingStarted = true;
            if (loading)
            {
                loading->setText("Logging in...");
            }
        }
        if (!this->httpRequestIsFinished())
        {
            if (loading)
            {
                loading->animate();
            }
        }
        else
        {
            if (loading)
            {
                loading->stop();
            }
            loadingStarted = false;
            FlipMapApp *app = static_cast<FlipMapApp *>(appContext);
            if (app->getHttpState() == ISSUE)
            {
                loginStatus = LoginRequestError;
                return;
            }
            char response[256];
            if (app && app->loadChar("login", response, sizeof(response)))
            {
                if (strstr(response, "[SUCCESS]") != NULL)
                {
                    loginStatus = LoginSuccess;
                    app->saveChar("login_status", "success");
                    locationStatus = LocationWaiting;
                    currentView = FlipMapRunViewLocation;
                    userRequest(RequestTypeLocationUpdate);
                }
                else if (strstr(response, "User not found") != NULL)
                {
                    loginStatus = LoginNotStarted;
                    currentView = FlipMapRunViewRegistration;
                    registrationStatus = RegistrationWaiting;
                    userRequest(RequestTypeRegistration);
                }
                else if (strstr(response, "Incorrect password") != NULL)
                {
                    loginStatus = LoginWrongPassword;
                }
                else if (strstr(response, "Username or password is empty.") != NULL)
                {
                    loginStatus = LoginCredentialsMissing;
                }
                else
                {
                    loginStatus = LoginRequestError;
                }
            }
            else
            {
                loginStatus = LoginRequestError;
            }
        }
        break;
    case LoginSuccess:
        canvas_draw_str(canvas, 0, 10, "Login successful!");
        canvas_draw_str(canvas, 0, 20, "Press OK to continue.");
        break;
    case LoginCredentialsMissing:
        canvas_draw_str(canvas, 0, 10, "Missing credentials!");
        canvas_draw_str(canvas, 0, 20, "Please set your username");
        canvas_draw_str(canvas, 0, 30, "and password in the app.");
        break;
    case LoginRequestError:
        canvas_draw_str(canvas, 0, 10, "Login request failed!");
        canvas_draw_str(canvas, 0, 20, "Check your network and");
        canvas_draw_str(canvas, 0, 30, "try again later.");
        break;
    case LoginWrongPassword:
        canvas_draw_str(canvas, 0, 10, "Wrong password!");
        canvas_draw_str(canvas, 0, 20, "Please check your password");
        canvas_draw_str(canvas, 0, 30, "and try again.");
        break;
    case LoginNotStarted:
        loginStatus = LoginWaiting;
        userRequest(RequestTypeLogin);
        break;
    default:
        canvas_draw_str(canvas, 0, 10, "Logging in...");
        break;
    }
}

void FlipMapRun::drawRegistrationView(Canvas *canvas)
{
    canvas_set_font(canvas, FontPrimary);
    static bool loadingStarted = false;
    switch (registrationStatus)
    {
    case RegistrationWaiting:
        if (!loadingStarted)
        {
            if (!loading)
            {
                loading = std::make_unique<Loading>(canvas);
            }
            loadingStarted = true;
            if (loading)
            {
                loading->setText("Registering...");
            }
        }
        if (!this->httpRequestIsFinished())
        {
            if (loading)
            {
                loading->animate();
            }
        }
        else
        {
            if (loading)
            {
                loading->stop();
            }
            loadingStarted = false;
            FlipMapApp *app = static_cast<FlipMapApp *>(appContext);
            if (app->getHttpState() == ISSUE)
            {
                registrationStatus = RegistrationRequestError;
                return;
            }
            char response[256];
            if (app && app->loadChar("register", response, sizeof(response)))
            {
                if (strstr(response, "[SUCCESS]") != NULL)
                {
                    registrationStatus = RegistrationSuccess;
                    locationStatus = LocationWaiting;
                    currentView = FlipMapRunViewLocation;
                    userRequest(RequestTypeLocationUpdate);
                }
                else if (strstr(response, "Username or password not provided") != NULL)
                {
                    registrationStatus = RegistrationCredentialsMissing;
                }
                else if (strstr(response, "User already exists") != NULL)
                {
                    registrationStatus = RegistrationUserExists;
                }
                else
                {
                    registrationStatus = RegistrationRequestError;
                }
            }
            else
            {
                registrationStatus = RegistrationRequestError;
            }
        }
        break;
    case RegistrationSuccess:
        canvas_draw_str(canvas, 0, 10, "Registration successful!");
        canvas_draw_str(canvas, 0, 20, "Press OK to continue.");
        break;
    case RegistrationCredentialsMissing:
        canvas_draw_str(canvas, 0, 10, "Missing credentials!");
        canvas_draw_str(canvas, 0, 20, "Please set your username");
        canvas_draw_str(canvas, 0, 30, "and password in the app.");
        break;
    case RegistrationRequestError:
        canvas_draw_str(canvas, 0, 10, "Registration request failed!");
        canvas_draw_str(canvas, 0, 20, "Check your network and");
        canvas_draw_str(canvas, 0, 30, "try again later.");
        break;
    default:
        canvas_draw_str(canvas, 0, 10, "Registering...");
        break;
    }
}

bool FlipMapRun::httpRequestIsFinished()
{
    FlipMapApp *app = static_cast<FlipMapApp *>(appContext);
    if (!app)
    {
        FURI_LOG_E(TAG, "httpRequestIsFinished: App context is NULL");
        return true;
    }
    auto state = app->getHttpState();
    return state == IDLE || state == ISSUE || state == INACTIVE;
}

void FlipMapRun::updateDraw(Canvas *canvas)
{
    canvas_clear(canvas);

    switch (currentView)
    {
    case FlipMapRunViewLogin:
        drawLoginView(canvas);
        break;
    case FlipMapRunViewRegistration:
        drawRegistrationView(canvas);
        break;
    case FlipMapRunViewMapData:
        // nothing yet
        canvas_draw_str(canvas, 0, 10, "Map Data View");
        break;
    case FlipMapRunViewLocation:
        drawLocationView(canvas);
        break;
    default:
        canvas_draw_str(canvas, 0, 10, "Unknown view");
        break;
    }
}

void FlipMapRun::updateInput(InputEvent *event)
{
    lastInput = event->key;
    debounceInput();

    switch (lastInput)
    {
    case InputKeyBack:
        // return to menu
        shouldReturnToMenu = true;
        break;
    default:
        // nothing else for now
        // but we could use it to move in the map
        break;
    };
}

void FlipMapRun::userRequest(RequestType requestType)
{
    FlipMapApp *app = static_cast<FlipMapApp *>(appContext);
    furi_check(app);

    // Allocate memory for credentials
    char *username = (char *)malloc(64);
    char *password = (char *)malloc(64);
    if (!username || !password)
    {
        FURI_LOG_E(TAG, "userRequest: Failed to allocate memory for credentials");
        if (username)
            free(username);
        if (password)
            free(password);
        return;
    }

    // Load credentials from storage
    bool credentialsLoaded = true;
    if (!app->loadChar("user_name", username, 64))
    {
        FURI_LOG_E(TAG, "Failed to load user_name");
        credentialsLoaded = false;
    }
    if (!app->loadChar("user_pass", password, 64))
    {
        FURI_LOG_E(TAG, "Failed to load user_pass");
        credentialsLoaded = false;
    }

    if (!credentialsLoaded)
    {
        switch (requestType)
        {
        case RequestTypeLogin:
            loginStatus = LoginCredentialsMissing;
            break;
        case RequestTypeRegistration:
            registrationStatus = RegistrationCredentialsMissing;
            break;
        case RequestTypeMapData:
            mapDataStatus = MapDataCredentialsMissing;
            break;
        case RequestTypeLocationUpdate:
            locationStatus = LocationCredentialsMissing;
            break;
        default:
            FURI_LOG_E(TAG, "Unknown request type: %d", requestType);
            loginStatus = LoginRequestError;
            registrationStatus = RegistrationRequestError;
            mapDataStatus = MapDataRequestError;
            locationStatus = LocationRequestError;
            break;
        }
        free(username);
        free(password);
        return;
    }

    // Create JSON payload for login/registration
    char *payload = (char *)malloc(256);
    if (!payload)
    {
        FURI_LOG_E(TAG, "userRequest: Failed to allocate memory for payload");
        free(username);
        free(password);
        return;
    }
    snprintf(payload, 256, "{\"username\":\"%s\",\"password\":\"%s\"}", username, password);

    switch (requestType)
    {
    case RequestTypeLogin:
        if (!app->httpRequestAsync("login.txt",
                                   "https://www.jblanked.com/flipper/api/user/login/",
                                   POST, "{\"Content-Type\":\"application/json\"}", payload))
        {
            loginStatus = LoginRequestError;
        }
        break;
    case RequestTypeRegistration:
        if (!app->httpRequestAsync("register.txt",
                                   "https://www.jblanked.com/flipper/api/user/register/",
                                   POST, "{\"Content-Type\":\"application/json\"}", payload))
        {
            registrationStatus = RegistrationRequestError;
        }
        break;
    case RequestTypeMapData:
        break;
    case RequestTypeLocationUpdate:
    {
        char *authHeaders = (char *)malloc(256);
        char *url = (char *)malloc(256);
        char *locationStatusStr = (char *)malloc(16);
        if (!authHeaders && !url && !locationStatusStr)
        {
            if (authHeaders)
                free(authHeaders);
            if (url)
                free(url);
            if (locationStatusStr)
                free(locationStatusStr);
            FURI_LOG_E(TAG, "userRequest: Failed to allocate memory for location update");
            locationStatus = LocationRequestError;
            return;
        }
        if (!app->loadChar("location_status", locationStatusStr, 16))
        {
            snprintf(locationStatusStr, 16, "Disabled");
        }
        snprintf(url, 256, "https://www.jblanked.com/flipper/api/user/location/update/%s/%s/", username, locationStatusStr);
        snprintf(authHeaders, 256, "{\"Content-Type\":\"application/json\", \"username\":\"%s\", \"password\":\"%s\"}", username, password);
        if (!app->httpRequestAsync("location_request.txt", url, GET, authHeaders, nullptr))
        {
            locationStatus = LocationRequestError;
        }
        free(authHeaders);
        free(url);
        free(locationStatusStr);
        break;
    }
    default:
        FURI_LOG_E(TAG, "Unknown request type: %d", requestType);
        loginStatus = LoginRequestError;
        registrationStatus = RegistrationRequestError;
        mapDataStatus = MapDataRequestError;
        locationStatus = LocationRequestError;
        break;
    }

    free(username);
    free(password);
    free(payload);
}