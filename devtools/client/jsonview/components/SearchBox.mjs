/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Component } from "resource://devtools/client/shared/vendor/react.mjs";
import PropTypes from "resource://devtools/client/shared/vendor/react-prop-types.mjs";
import * as dom from "resource://devtools/client/shared/vendor/react-dom-factories.mjs";

const { input, div, button } = dom;

// For smooth incremental searching (in case the user is typing quickly).
const searchDelay = 250;

/**
 * This object represents a search box located at the
 * top right corner of the application.
 */
class SearchBox extends Component {
  static get propTypes() {
    return {
      actions: PropTypes.object,
      value: PropTypes.toString,
    };
  }

  constructor(props) {
    super(props);
    this.onSearch = this.onSearch.bind(this);
    this.doSearch = this.doSearch.bind(this);
    this.onClearButtonClick = this.onClearButtonClick.bind(this);
    this.onKeyDown = this.onKeyDown.bind(this);

    this.state = {
      value: props.value || "",
    };
  }

  onSearch(event) {
    this.setState({
      value: event.target.value,
    });
    const searchBox = event.target;
    const win = searchBox.ownerDocument.defaultView;

    if (this.searchTimeout) {
      win.clearTimeout(this.searchTimeout);
    }

    const callback = this.doSearch.bind(this, searchBox);
    this.searchTimeout = win.setTimeout(callback, searchDelay);
  }

  doSearch(searchBox) {
    this.props.actions.onSearch(searchBox.value);
  }

  onClearButtonClick() {
    this.setState({ value: "" });
    this.props.actions.onSearch("");
    if (this._searchBoxRef) {
      this._searchBoxRef.focus();
    }
  }

  onKeyDown(e) {
    switch (e.key) {
      case "Escape":
        e.preventDefault();
        this.onClearButtonClick();
        break;
    }
  }

  render() {
    const { value } = this.state;
    return div(
      { className: "devtools-searchbox" },
      input({
        className: "searchBox devtools-filterinput",
        placeholder: JSONView.Locale["jsonViewer.filterJSON"],
        onChange: this.onSearch,
        onKeyDown: this.onKeyDown,
        value,
        ref: c => {
          this._searchBoxRef = c;
        },
      }),
      button({
        className: "devtools-searchinput-clear",
        hidden: !value,
        onClick: this.onClearButtonClick,
      })
    );
  }
}

export default { SearchBox };
