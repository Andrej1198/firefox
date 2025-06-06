/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// React
const {
  Component,
} = require("resource://devtools/client/shared/vendor/react.mjs");
const {
  span,
} = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.mjs");

class Badge extends Component {
  static get propTypes() {
    return {
      score: PropTypes.string.isRequired,
      label: PropTypes.string.isRequired,
      ariaLabel: PropTypes.string,
      tooltip: PropTypes.string,
    };
  }

  render() {
    const { score, label, ariaLabel, tooltip } = this.props;

    return span(
      {
        className: `audit-badge badge`,
        "data-score": score,
        title: tooltip,
        "aria-label": ariaLabel || label,
      },
      label
    );
  }
}

module.exports = Badge;
