// Copyright (C) 2020—2021 Andrej Shadura
// SPDX-License-Identifier: MIT
mod mask_iter;
use mask_iter::IterableMask;
use nix::unistd::daemon;
#[cfg(feature = "pidfile")]
use pidfile_rs::Pidfile;
use std::convert::{From, TryFrom};
use structopt::StructOpt;

#[cfg(feature = "pidfile")]
use std::{fs::Permissions, os::unix::fs::PermissionsExt, path::PathBuf};

use std::process::Command;

use anyhow::{anyhow, Context, Result};

use x11rb::connection::{
    Connection as _, RequestConnection
};
use x11rb::protocol::Event;
use x11rb::protocol::xinput::{
    self, ConnectionExt as _,
    Device, DeviceId, DeviceType, EventMask,
    HierarchyInfo, HierarchyMask,
    XIDeviceInfo, XIEventMask
};
use x11rb::protocol::xproto::GE_GENERIC_EVENT;

#[derive(Debug, StructOpt)]
#[structopt(name = "inputplug", about = "XInput event monitor")]
struct Opt {
    /// Enable debug mode.
    #[structopt(long)]
    debug: bool,

    /// Be a bit more verbose.
    #[structopt(short, long)]
    verbose: bool,

    /// Don't daemonize, run in the foreground.
    #[structopt(short = "d", long)]
    foreground: bool,

    /// Start up, monitor events, but don't actually run anything.
    ///
    /// With verbose more enabled, would print the actual command it'd run.
    #[structopt(short, long)]
    no_act: bool,

    /// On start, trigger added and enabled events for each plugged devices.
    ///
    /// A master device will trigger the "added" event while a slave
    /// device will trigger both the "added" and the "enabled" device.
    #[structopt(short = "0", long)]
    bootstrap: bool,

    /// Command prefix to run.
    #[structopt(short = "c", long)]
    command: String,

    /// PID file
    #[cfg(feature = "pidfile")]
    #[structopt(short = "p", long, parse(from_os_str))]
    pidfile: Option<PathBuf>,
}

trait HierarchyChangeEvent<T> {
    fn to_cmdline(&self, conn: &impl RequestConnection) -> Vec<String>;
}

fn device_name(conn: &impl RequestConnection, deviceid: DeviceId) -> Option<String> {
    if let Ok(r) = conn.xinput_xi_query_device(deviceid) {
        if let Ok(reply) = r.reply() {
            reply.infos.iter()
                .find(|info| info.deviceid == deviceid)
                .map(|info| String::from_utf8_lossy(&info.name).to_string())
        } else {
            None
        }
    } else {
        None
    }
}

fn format_device_type(device_type: DeviceType) -> String {
    if device_type == DeviceType::from(0u8) {
        "".into()
    } else {
        format!("XI{:#?}", device_type)
    }
}

impl<T> HierarchyChangeEvent<T> for XIDeviceInfo {
    fn to_cmdline(&self, conn: &impl RequestConnection) -> Vec<String> {
        vec![
            self.deviceid.to_string(),
            format_device_type(self.type_),
            String::from_utf8_lossy(&self.name).to_string(),
        ]
    }
}

impl<T> HierarchyChangeEvent<T> for HierarchyInfo {
    fn to_cmdline(&self, conn: &impl RequestConnection) -> Vec<String> {
        vec![
            self.deviceid.to_string(),
            format_device_type(self.type_),
            device_name(conn, self.deviceid).unwrap_or("".to_string()),
        ]
    }
}

fn handle_device<T: HierarchyChangeEvent<T>>(
    opt: &Opt,
    conn: &impl RequestConnection,
    device_info: &T,
    change: HierarchyMask
) {
    let mut command = Command::new(&opt.command);

    command.arg(format!("XI{:#?}", change))
           .args(device_info.to_cmdline(conn));
    if opt.verbose {
        println!("{:?}", &command);
    }
    if !opt.no_act {
        if let Err(e) = command.status() {
            eprintln!("Command failed: {}", e);
        }
    }
}

fn main() -> Result<()> {
    let opt = Opt::from_args();

    let (conn, _) = x11rb::connect(None).context("Can't open X display")?;

    let xinput_info = conn
        .extension_information(xinput::X11_EXTENSION_NAME)
        .context("X Input extension cannot be detected.")?
        .ok_or(anyhow!("X Input extension not available."))?;

    if opt.debug {
        println!("X Input extension opcode: {}", xinput_info.major_opcode);
    }

    // We don’t want to inherit an open connection into the daemon
    drop(conn);

    #[cfg(feature = "pidfile")]
    let pidfile = if opt.pidfile.is_some() {
        Some(Pidfile::new(
            &opt.pidfile.as_ref().unwrap(),
            Permissions::from_mode(0o600)
        )?)
    } else {
        None
    };

    if !opt.foreground {
        daemon(false, opt.verbose).context("Cannot daemonize")?;

        println!("Daemonized.");

        #[cfg(feature = "pidfile")]
        if pidfile.is_some() {
            if let Err(error) = pidfile.unwrap().write() {
                eprintln!("Failed to write to the PID file: {:?}", error);
            }
        }
    }

    // Now that we’re in the daemon, reconnect to the X server
    let (conn, screen_num) = x11rb::connect(None)
        .context("Can't reconnect to the X display")?;

    let screen = &conn.setup().roots[screen_num];

    if opt.bootstrap {
        if opt.debug {
            println!("Bootstrapping events");
        }

        if let Ok(reply) = conn.xinput_xi_query_device(bool::from(Device::ALL)) {
            let reply = reply.reply()?;
            for info in reply.infos {
                match DeviceType::try_from(info.type_).unwrap() {
                    DeviceType::MASTER_POINTER |
                    DeviceType::MASTER_KEYBOARD => {
                        handle_device(&opt, &conn, &info, HierarchyMask::MASTER_ADDED)
                    }
                    DeviceType::SLAVE_POINTER |
                    DeviceType::SLAVE_KEYBOARD |
                    DeviceType::FLOATING_SLAVE => {
                        handle_device(&opt, &conn, &info, HierarchyMask::SLAVE_ADDED);
                        handle_device(&opt, &conn, &info, HierarchyMask::DEVICE_ENABLED)
                    }
                    _ => {}
                }
            }
        }
    }

    conn.xinput_xi_select_events(
        screen.root,
        &[EventMask {
            deviceid: bool::from(Device::ALL).into(),
            mask: vec![XIEventMask::HIERARCHY.into()],
        }]
    )?;

    conn.flush()?;
    loop {
        let event = conn.wait_for_event()
            .context("Failed to get an event")?;
        if event.response_type() != GE_GENERIC_EVENT {
            continue;
        }
        if let Event::XinputHierarchy(hier_event) = event {
            if hier_event.extension != xinput_info.major_opcode {
                continue;
            }
            for info in hier_event.infos {
                let flags = IterableMask::from(u32::from(info.flags))
                    .map(|x| HierarchyMask::from(x as u8))
                    .collect::<Vec<HierarchyMask>>();

                for flag in flags {
                    handle_device(&opt, &conn, &info, flag);
                }
            }
        }
    }
}
