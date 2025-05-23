/* vim: se cin sw=2 ts=2 et : */
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIWinTaskbar.h"
#include "WinTaskbar.h"
#include "TaskbarPreview.h"
#include "nsITaskbarPreviewController.h"

#include "mozilla/RefPtr.h"
#include "mozilla/widget/JumpListBuilder.h"
#include <nsError.h>
#include <nsCOMPtr.h>
#include <nsIWidget.h>
#include <nsIBaseWindow.h>
#include <nsServiceManagerUtils.h>
#include "nsIXULAppInfo.h"
#include "nsWindow.h"
#include "WinUtils.h"
#include "TaskbarTabPreview.h"
#include "TaskbarWindowPreview.h"
#include "nsWidgetsCID.h"
#include "nsPIDOMWindow.h"
#include "nsAppDirectoryServiceDefs.h"
#include "mozilla/Preferences.h"
#include "nsAppRunner.h"
#include "nsXREDirProvider.h"
#include "mozilla/widget/WinRegistry.h"
#include <io.h>
#include <propvarutil.h>
#include <propkey.h>
#include <shellapi.h>

namespace {

HWND GetHWNDFromDocShell(nsIDocShell* aShell) {
  nsCOMPtr<nsIBaseWindow> baseWindow(
      do_QueryInterface(reinterpret_cast<nsISupports*>(aShell)));

  if (!baseWindow) return nullptr;

  nsCOMPtr<nsIWidget> widget;
  baseWindow->GetMainWidget(getter_AddRefs(widget));

  return widget ? (HWND)widget->GetNativeData(NS_NATIVE_WINDOW) : nullptr;
}

HWND GetHWNDFromDOMWindow(mozIDOMWindow* dw) {
  nsCOMPtr<nsIWidget> widget;

  if (!dw) return nullptr;

  nsCOMPtr<nsPIDOMWindowInner> window = nsPIDOMWindowInner::From(dw);
  return GetHWNDFromDocShell(window->GetDocShell());
}

nsresult SetWindowAppUserModelProp(mozIDOMWindow* aParent,
                                   const nsString& aIdentifier) {
  NS_ENSURE_ARG_POINTER(aParent);

  if (aIdentifier.IsEmpty()) return NS_ERROR_INVALID_ARG;

  HWND toplevelHWND = ::GetAncestor(GetHWNDFromDOMWindow(aParent), GA_ROOT);

  if (!toplevelHWND) return NS_ERROR_INVALID_ARG;

  RefPtr<IPropertyStore> pPropStore;
  if (FAILED(SHGetPropertyStoreForWindow(toplevelHWND, IID_IPropertyStore,
                                         getter_AddRefs(pPropStore)))) {
    return NS_ERROR_INVALID_ARG;
  }

  PROPVARIANT pv;
  if (FAILED(InitPropVariantFromString(aIdentifier.get(), &pv))) {
    return NS_ERROR_UNEXPECTED;
  }

  nsresult rv = NS_OK;
  if (FAILED(pPropStore->SetValue(PKEY_AppUserModel_ID, pv)) ||
      FAILED(pPropStore->Commit())) {
    rv = NS_ERROR_FAILURE;
  }

  PropVariantClear(&pv);

  return rv;
}

///////////////////////////////////////////////////////////////////////////////
// default nsITaskbarPreviewController

class DefaultController final : public nsITaskbarPreviewController {
  ~DefaultController() {}
  HWND mWnd;

 public:
  explicit DefaultController(HWND hWnd) : mWnd(hWnd) {}

  NS_DECL_ISUPPORTS
  NS_DECL_NSITASKBARPREVIEWCONTROLLER
};

NS_IMETHODIMP
DefaultController::GetWidth(uint32_t* aWidth) {
  RECT r;
  ::GetClientRect(mWnd, &r);
  *aWidth = r.right;
  return NS_OK;
}

NS_IMETHODIMP
DefaultController::GetHeight(uint32_t* aHeight) {
  RECT r;
  ::GetClientRect(mWnd, &r);
  *aHeight = r.bottom;
  return NS_OK;
}

NS_IMETHODIMP
DefaultController::GetThumbnailAspectRatio(float* aThumbnailAspectRatio) {
  uint32_t width, height;
  GetWidth(&width);
  GetHeight(&height);
  if (!height) height = 1;

  *aThumbnailAspectRatio = width / float(height);
  return NS_OK;
}

NS_IMETHODIMP
DefaultController::RequestThumbnail(nsITaskbarPreviewCallback* aCallback,
                                    uint32_t width, uint32_t height) {
  return NS_OK;
}

NS_IMETHODIMP
DefaultController::RequestPreview(nsITaskbarPreviewCallback* aCallback) {
  return NS_OK;
}

NS_IMETHODIMP
DefaultController::OnClose(void) {
  MOZ_ASSERT_UNREACHABLE(
      "OnClose should not be called for "
      "TaskbarWindowPreviews");
  return NS_OK;
}

NS_IMETHODIMP
DefaultController::OnActivate(bool* rAcceptActivation) {
  *rAcceptActivation = true;
  MOZ_ASSERT_UNREACHABLE(
      "OnActivate should not be called for "
      "TaskbarWindowPreviews");
  return NS_OK;
}

NS_IMETHODIMP
DefaultController::OnClick(nsITaskbarPreviewButton* button) { return NS_OK; }

NS_IMPL_ISUPPORTS(DefaultController, nsITaskbarPreviewController)
}  // namespace

namespace mozilla {
namespace widget {

///////////////////////////////////////////////////////////////////////////////
// nsIWinTaskbar

NS_IMPL_ISUPPORTS(WinTaskbar, nsIWinTaskbar)

bool WinTaskbar::Initialize() {
  if (mTaskbar) return true;

  ::CoInitialize(nullptr);
  HRESULT hr =
      ::CoCreateInstance(CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER,
                         IID_ITaskbarList4, (void**)&mTaskbar);
  if (FAILED(hr)) return false;

  hr = mTaskbar->HrInit();
  if (FAILED(hr)) {
    // This may fail with shell extensions like blackbox installed.
    NS_WARNING("Unable to initialize taskbar");
    NS_RELEASE(mTaskbar);
    return false;
  }
  return true;
}

WinTaskbar::WinTaskbar() : mTaskbar(nullptr) {}

WinTaskbar::~WinTaskbar() {
  if (mTaskbar) {  // match successful Initialize() call
    NS_RELEASE(mTaskbar);
    ::CoUninitialize();
  }
}

// static
bool WinTaskbar::GenerateAppUserModelID(nsAString& aAppUserModelId,
                                        bool aPrivateBrowsing) {
  // If marked as such in prefs, use a hash of the profile path for the id
  // instead of the install path hash setup by the installer.
  if (Preferences::GetBool("taskbar.grouping.useprofile", false)) {
    nsCOMPtr<nsIFile> profileDir;
    NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                           getter_AddRefs(profileDir));
    bool exists = false;
    if (profileDir && NS_SUCCEEDED(profileDir->Exists(&exists)) && exists) {
      nsAutoCString path;
      if (NS_SUCCEEDED(profileDir->GetPersistentDescriptor(path))) {
        nsAutoString id;
        id.AppendInt(HashString(path));
        if (!id.IsEmpty()) {
          aAppUserModelId.Assign(id);

          if (aPrivateBrowsing) {
            aAppUserModelId.AppendLiteral(";PrivateBrowsingAUMID");
          }

          return true;
        }
      }
    }
  }

  // The default value is set by the installer and is stored in the registry
  // under (HKLM||HKCU)/Software/Mozilla/Firefox/TaskBarIDs. If for any reason
  // hash generation operation fails, the installer will not store a value in
  // the registry or set ids on shortcuts. A lack of an id can also occur for
  // zipped builds.
  nsCOMPtr<nsIXULAppInfo> appInfo =
      do_GetService("@mozilla.org/xre/app-info;1");
  nsCString appName;
  if (appInfo && NS_SUCCEEDED(appInfo->GetName(appName))) {
    nsAutoString regKey;
    regKey.AssignLiteral("Software\\Mozilla\\");
    AppendASCIItoUTF16(appName, regKey);
    regKey.AppendLiteral("\\TaskBarIDs");

    WCHAR path[MAX_PATH];
    if (GetModuleFileNameW(nullptr, path, MAX_PATH)) {
      wchar_t* slash = wcsrchr(path, '\\');
      if (!slash) return false;
      *slash = '\0';  // no trailing slash

      nsDependentString pathStr(path);
      for (auto* rootKey : {HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER}) {
        if (auto aumid = WinRegistry::GetString(rootKey, regKey, pathStr)) {
          aAppUserModelId = std::move(*aumid);
          break;
        }
      }
    }
  }

  // If we haven't found an ID yet then use the install hash. In xpcshell tests
  // the directory provider may not have been initialized so bypass in this
  // case.
  if (aAppUserModelId.IsEmpty() && gDirServiceProvider) {
    gDirServiceProvider->GetInstallHash(aAppUserModelId);
  }

  if (aPrivateBrowsing) {
    aAppUserModelId.AppendLiteral(";PrivateBrowsingAUMID");
  }

  return !aAppUserModelId.IsEmpty();
}

// static
bool WinTaskbar::GetAppUserModelID(nsAString& aAppUserModelId,
                                   bool aPrivateBrowsing) {
  // If an ID has already been set then use that.
  PWSTR id;
  if (SUCCEEDED(GetCurrentProcessExplicitAppUserModelID(&id))) {
    aAppUserModelId.Assign(id);
    CoTaskMemFree(id);
  }

  return GenerateAppUserModelID(aAppUserModelId, aPrivateBrowsing);
}

NS_IMETHODIMP
WinTaskbar::GetDefaultGroupId(nsAString& aDefaultGroupId) {
  if (!GetAppUserModelID(aDefaultGroupId)) return NS_ERROR_UNEXPECTED;

  return NS_OK;
}

NS_IMETHODIMP
WinTaskbar::GetDefaultPrivateGroupId(nsAString& aDefaultPrivateGroupId) {
  if (!GetAppUserModelID(aDefaultPrivateGroupId, true))
    return NS_ERROR_UNEXPECTED;

  return NS_OK;
}

// (static) Called from AppShell
bool WinTaskbar::RegisterAppUserModelID() {
  nsAutoString uid;
  if (!GetAppUserModelID(uid)) return false;

  return SUCCEEDED(SetCurrentProcessExplicitAppUserModelID(uid.get()));
}

NS_IMETHODIMP
WinTaskbar::GetAvailable(bool* aAvailable) {
  // ITaskbarList4::HrInit() may fail with shell extensions like blackbox
  // installed. Initialize early to return available=false in those cases.
  *aAvailable = Initialize();

  return NS_OK;
}

NS_IMETHODIMP
WinTaskbar::CreateTaskbarTabPreview(nsIDocShell* shell,
                                    nsITaskbarPreviewController* controller,
                                    nsITaskbarTabPreview** _retval) {
  if (!Initialize()) return NS_ERROR_NOT_AVAILABLE;

  NS_ENSURE_ARG_POINTER(shell);
  NS_ENSURE_ARG_POINTER(controller);

  HWND toplevelHWND = ::GetAncestor(GetHWNDFromDocShell(shell), GA_ROOT);

  if (!toplevelHWND) return NS_ERROR_INVALID_ARG;

  RefPtr<TaskbarTabPreview> preview(
      new TaskbarTabPreview(mTaskbar, controller, toplevelHWND, shell));
  if (!preview) return NS_ERROR_OUT_OF_MEMORY;

  nsresult rv = preview->Init();
  if (NS_FAILED(rv)) {
    return rv;
  }

  preview.forget(_retval);

  return NS_OK;
}

NS_IMETHODIMP
WinTaskbar::GetTaskbarWindowPreview(nsIDocShell* shell,
                                    nsITaskbarWindowPreview** _retval) {
  if (!Initialize()) return NS_ERROR_NOT_AVAILABLE;

  NS_ENSURE_ARG_POINTER(shell);

  HWND toplevelHWND = ::GetAncestor(GetHWNDFromDocShell(shell), GA_ROOT);

  if (!toplevelHWND) return NS_ERROR_INVALID_ARG;

  nsWindow* window = WinUtils::GetNSWindowPtr(toplevelHWND);

  if (!window) return NS_ERROR_FAILURE;

  nsCOMPtr<nsITaskbarWindowPreview> preview = window->GetTaskbarPreview();
  if (!preview) {
    RefPtr<DefaultController> defaultController =
        new DefaultController(toplevelHWND);

    TaskbarWindowPreview* previewRaw = new TaskbarWindowPreview(
        mTaskbar, defaultController, toplevelHWND, shell);
    if (!previewRaw) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    preview = previewRaw;

    nsresult rv = previewRaw->Init();
    if (NS_FAILED(rv)) {
      return rv;
    }
    window->SetTaskbarPreview(preview);
  }

  preview.forget(_retval);

  return NS_OK;
}

NS_IMETHODIMP
WinTaskbar::GetTaskbarProgress(nsIDocShell* shell,
                               nsITaskbarProgress** _retval) {
  nsCOMPtr<nsITaskbarWindowPreview> preview;
  nsresult rv = GetTaskbarWindowPreview(shell, getter_AddRefs(preview));
  NS_ENSURE_SUCCESS(rv, rv);

  return CallQueryInterface(preview, _retval);
}

NS_IMETHODIMP
WinTaskbar::GetOverlayIconController(
    nsIDocShell* shell, nsITaskbarOverlayIconController** _retval) {
  nsCOMPtr<nsITaskbarWindowPreview> preview;
  nsresult rv = GetTaskbarWindowPreview(shell, getter_AddRefs(preview));
  NS_ENSURE_SUCCESS(rv, rv);

  return CallQueryInterface(preview, _retval);
}

NS_IMETHODIMP
WinTaskbar::CreateJumpListBuilder(bool aPrivateBrowsing,
                                  nsIJumpListBuilder** aJumpListBuilder) {
  nsAutoString aumid;
  GenerateAppUserModelID(aumid, aPrivateBrowsing);

  nsCOMPtr<nsIJumpListBuilder> builder = new JumpListBuilder(aumid);
  if (!builder) {
    return NS_ERROR_UNEXPECTED;
  }

  NS_IF_ADDREF(*aJumpListBuilder = builder);
  return NS_OK;
}

NS_IMETHODIMP
WinTaskbar::GetGroupIdForWindow(mozIDOMWindow* aParent,
                                nsAString& aIdentifier) {
  NS_ENSURE_ARG_POINTER(aParent);
  HWND toplevelHWND = ::GetAncestor(GetHWNDFromDOMWindow(aParent), GA_ROOT);
  if (!toplevelHWND) return NS_ERROR_INVALID_ARG;
  RefPtr<IPropertyStore> pPropStore;
  if (FAILED(SHGetPropertyStoreForWindow(toplevelHWND, IID_IPropertyStore,
                                         getter_AddRefs(pPropStore)))) {
    return NS_ERROR_INVALID_ARG;
  }
  PROPVARIANT pv;
  PropVariantInit(&pv);
  auto cleanupPropVariant = MakeScopeExit([&] { PropVariantClear(&pv); });
  if (FAILED(pPropStore->GetValue(PKEY_AppUserModel_ID, &pv))) {
    return NS_ERROR_FAILURE;
  }
  if (pv.vt != VT_LPWSTR) {
    // This can happen when there is no window specific group ID set
    // It's not an error case so we have to check for empty strings
    // returned from the function.
    return NS_OK;
  }
  aIdentifier.Assign(char16ptr_t(pv.pwszVal));
  return NS_OK;
}

NS_IMETHODIMP
WinTaskbar::SetGroupIdForWindow(mozIDOMWindow* aParent,
                                const nsAString& aIdentifier) {
  return SetWindowAppUserModelProp(aParent, nsString(aIdentifier));
}

NS_IMETHODIMP
WinTaskbar::PrepareFullScreen(void* aHWND, bool aFullScreen) {
  if (!Initialize()) return NS_ERROR_NOT_AVAILABLE;

  NS_ENSURE_ARG_POINTER(aHWND);

  if (!::IsWindow((HWND)aHWND)) return NS_ERROR_INVALID_ARG;

  HRESULT hr = mTaskbar->MarkFullscreenWindow((HWND)aHWND, aFullScreen);
  if (FAILED(hr)) {
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

}  // namespace widget
}  // namespace mozilla
