/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! Support for legacy telemetry ping creation. The ping supports serialization which should be
//! used when submitting.

use crate::logic::annotations;
use crate::std;
use anyhow::Context;
use serde::Serialize;
use std::collections::BTreeMap;
use time::format_description::well_known::{iso8601, Iso8601};
use uuid::Uuid;

const TELEMETRY_VERSION: u64 = 4;
const PAYLOAD_VERSION: u64 = 1;

// We need a custom time serializer to encode at most 3 decimal digits in the fractions of a second
// (millisecond precision).
const TIME_CONFIG: iso8601::EncodedConfig = iso8601::Config::DEFAULT
    .set_time_precision(iso8601::TimePrecision::Second {
        // Safety: 3 is non-zero
        decimal_digits: Some(unsafe { std::num::NonZeroU8::new_unchecked(3) }),
    })
    .encode();
const TIME_FORMAT: Iso8601<TIME_CONFIG> = Iso8601::<TIME_CONFIG>;
time::serde::format_description!(time_format, OffsetDateTime, TIME_FORMAT);

#[derive(Serialize)]
#[serde(
    tag = "type",
    rename_all = "camelCase",
    rename_all_fields = "camelCase"
)]
pub enum Ping<'a> {
    Crash {
        id: &'a Uuid,
        version: u64,
        #[serde(with = "time_format")]
        creation_date: time::OffsetDateTime,
        client_id: &'a str,
        profile_group_id: &'a str,
        #[serde(skip_serializing_if = "serde_json::Value::is_null")]
        environment: serde_json::Value,
        payload: Payload<'a>,
        application: Application<'a>,
    },
}

time::serde::format_description!(date_format, Date, "[year]-[month]-[day]");

#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
pub struct Payload<'a> {
    session_id: &'a str,
    version: u64,
    #[serde(with = "date_format")]
    crash_date: time::Date,
    #[serde(with = "time_format")]
    crash_time: time::OffsetDateTime,
    has_crash_environment: bool,
    crash_id: &'a str,
    minidump_sha256_hash: Option<&'a str>,
    process_type: &'a str,
    #[serde(skip_serializing_if = "serde_json::Value::is_null")]
    stack_traces: serde_json::Value,
    metadata: BTreeMap<&'a str, &'a str>,
}

#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
pub struct Application<'a> {
    vendor: &'a str,
    name: &'a str,
    build_id: &'a str,
    display_version: String,
    platform_version: String,
    version: &'a str,
    channel: &'a str,
    #[serde(skip_serializing_if = "Option::is_none")]
    architecture: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    xpcom_abi: Option<String>,
}

impl<'a> Ping<'a> {
    pub fn crash(
        ping_id: &'a Uuid,
        extra: &'a serde_json::Value,
        crash_id: &'a str,
        minidump_sha256_hash: Option<&'a str>,
    ) -> anyhow::Result<Self> {
        let now: time::OffsetDateTime = crate::std::time::SystemTime::now().into();
        let environment: serde_json::Value = extra["TelemetryEnvironment"]
            .as_str()
            .and_then(|estr| serde_json::from_str(estr).ok())
            .unwrap_or_default();

        // The subset of extra file entries (crash annotations) which are allowed in pings.
        let metadata = extra
            .as_object()
            .map(|map| {
                map.iter()
                    .filter_map(|(k, v)| {
                        annotations::send_in_ping(k)
                            .then(|| k.as_str())
                            .zip(v.as_str())
                    })
                    .collect()
            })
            .unwrap_or_default();

        let display_version = environment
            .pointer("/build/displayVersion")
            .and_then(|s| s.as_str())
            .unwrap_or_default()
            .to_owned();
        let platform_version = environment
            .pointer("/build/platformVersion")
            .and_then(|s| s.as_str())
            .unwrap_or_default()
            .to_owned();
        let architecture = environment
            .pointer("/build/architecture")
            .and_then(|s| s.as_str())
            .map(ToOwned::to_owned);
        let xpcom_abi = environment
            .pointer("/build/xpcomAbi")
            .and_then(|s| s.as_str())
            .map(ToOwned::to_owned);

        Ok(Ping::Crash {
            id: ping_id,
            version: TELEMETRY_VERSION,
            creation_date: now,
            client_id: extra["TelemetryClientId"]
                .as_str()
                .context("missing TelemetryClientId")?,
            profile_group_id: extra["TelemetryProfileGroupId"]
                .as_str()
                .context("missing TelemetryProfileGroupId")?,
            environment,
            payload: Payload {
                session_id: extra["TelemetrySessionId"]
                    .as_str()
                    .context("missing TelemetrySessionId")?,
                version: PAYLOAD_VERSION,
                crash_date: now.date(),
                crash_time: now,
                has_crash_environment: true,
                crash_id,
                minidump_sha256_hash,
                process_type: "main",
                stack_traces: extra["StackTraces"].clone(),
                metadata,
            },
            application: Application {
                vendor: extra["Vendor"].as_str().unwrap_or_default(),
                name: extra["ProductName"].as_str().unwrap_or_default(),
                build_id: extra["BuildID"].as_str().unwrap_or_default(),
                display_version,
                platform_version,
                version: extra["Version"].as_str().unwrap_or_default(),
                channel: extra["ReleaseChannel"].as_str().unwrap_or_default(),
                architecture,
                xpcom_abi,
            },
        })
    }

    /// Generate the telemetry URL for submitting this ping.
    pub fn submission_url(&self, extra: &serde_json::Value) -> anyhow::Result<String> {
        let url = extra["TelemetryServerURL"]
            .as_str()
            .context("missing TelemetryServerURL")?;
        let id = match self {
            Self::Crash { id, .. } => id,
        };
        let name = extra["ProductName"]
            .as_str()
            .context("missing ProductName")?;
        let version = extra["Version"].as_str().context("missing Version")?;
        let channel = extra["ReleaseChannel"]
            .as_str()
            .context("missing ReleaseChannel")?;
        let buildid = extra["BuildID"].as_str().context("missing BuildID")?;
        Ok(format!("{url}/submit/telemetry/{id}/crash/{name}/{version}/{channel}/{buildid}?v={TELEMETRY_VERSION}"))
    }
}
