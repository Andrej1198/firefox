/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cm = Components.manager;

export var MockRegistrar = Object.freeze({
  _registeredComponents: new Map(),
  _originalCIDs: new Map(),
  get registrar() {
    return Cm.QueryInterface(Ci.nsIComponentRegistrar);
  },

  /**
   * Register a mock to override target interfaces.
   * The target interface may be accessed through _genuine property of the mock.
   * If you register multiple mocks to the same contract ID, you have to call
   * unregister in reverse order. Otherwise the previous factory will not be
   * restored.
   *
   * @param contractID The contract ID of the interface which is overridden by
                       the mock.
   *                   e.g. "@mozilla.org/file/directory_service;1"
   * @param mock       An object which implements interfaces for the contract ID.
   * @param args       An array which is passed in the constructor of mock.
   *
   * @return           The CID of the mock.
   */
  register(contractID, mock, args) {
    return this.registerEx(
      contractID,
      { shouldCreateInstance: true },
      mock,
      args
    );
  },

  /**
   * Register a mock to override target interfaces.
   * If shouldCreateInstance is true then the target interface may be accessed
   * through _genuine property of the mock.
   * If you register multiple mocks to the same contract ID, you have to call
   * unregister in reverse order. Otherwise the previous factory will not be
   * restored.
   *
   * @param contractID The contract ID of the interface which is overridden by
                       the mock.
   *                   e.g. "@mozilla.org/file/directory_service;1"
   * @param options    Options object with any of the following optional
   *                   parameters:
   *                   * shouldCreateInstance: Adds the _genuine property to
   *                     the mock.
   * @param mock       An object which implements interfaces for the contract ID.
   * @param args       An array which is passed in the constructor of mock.
   *
   * @return           The CID of the mock.
   */
  registerEx(contractID, options, mock, args) {
    let originalCID;
    let originalFactory;
    try {
      originalCID = this._originalCIDs.get(contractID);
      if (!originalCID) {
        originalCID = this.registrar.contractIDToCID(contractID);
        this._originalCIDs.set(contractID, originalCID);
      }

      originalFactory = Cm.getClassObject(originalCID, Ci.nsIFactory);
    } catch (e) {
      // There's no original factory. Ignore and just register the new
      // one.
    }

    let cid = Services.uuid.generateUUID();

    let factory = {
      createInstance(iid) {
        let wrappedMock;
        if (mock.prototype && mock.prototype.constructor) {
          wrappedMock = Object.create(mock.prototype);
          mock.apply(wrappedMock, args);
        } else if (typeof mock == "function") {
          wrappedMock = mock();
        } else {
          wrappedMock = mock;
        }

        if (originalFactory && options.shouldCreateInstance) {
          try {
            let genuine = originalFactory.createInstance(iid);
            wrappedMock._genuine = genuine;
          } catch (ex) {
            console.error(
              "MockRegistrar: Creating original instance failed",
              ex
            );
          }
        }

        return wrappedMock.QueryInterface(iid);
      },
      QueryInterface: ChromeUtils.generateQI(["nsIFactory"]),
    };

    this.registrar.registerFactory(
      cid,
      "A Mock for " + contractID,
      contractID,
      factory
    );

    this._registeredComponents.set(cid, {
      contractID,
      factory,
      originalCID,
    });

    return cid;
  },

  /**
   * Unregister the mock.
   *
   * @param cid The CID of the mock.
   */
  unregister(cid) {
    let component = this._registeredComponents.get(cid);
    if (!component) {
      return;
    }

    this.registrar.unregisterFactory(cid, component.factory);
    if (component.originalCID) {
      // Passing `null` for the factory re-maps the contract ID to the
      // entry for its original CID.
      this.registrar.registerFactory(
        component.originalCID,
        "",
        component.contractID,
        null
      );
    }

    this._registeredComponents.delete(cid);
  },

  /**
   * Unregister all registered mocks.
   */
  unregisterAll() {
    for (let cid of this._registeredComponents.keys()) {
      this.unregister(cid);
    }
  },
});
