mod mask_iter;
use mask_iter::IterableMask;
use structopt::StructOpt;
use std::convert::{TryFrom, From};
use std::path::PathBuf;
use std::process::Command;
use std::process::exit;
use x11rb::connection::{
    Connection as _, RequestConnection
};
use x11rb::generated::xproto::GE_GENERIC_EVENT;
use x11rb::generated::xinput::{
    self, ConnectionExt as _,
    Device, DeviceId, DeviceType, EventMask,
    HierarchyEvent, HierarchyInfo, HierarchyMask,
    XIDeviceInfo, XIEventMask
};
use x11rb::x11_utils::Event;

#[derive(Debug, StructOpt)]
#[structopt(name = "inputplug", about = "XInput event monitor.")]
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
    #[structopt(short = "p", long, parse(from_os_str))]
    pidfile: Option<PathBuf>,
}

fn daemon(nochdir: bool, noclose: bool) -> Result<(), std::io::Error> {
    let res = unsafe {
        libc::daemon(nochdir as libc::c_int, noclose as libc::c_int)
    };
    if res != 0 {
        Err(std::io::Error::last_os_error())
    } else {
        Ok(())
    }
}

trait HierarchyChangeEvent<T> {
    fn to_cmdline(&self, conn: &impl RequestConnection) -> Vec<String>;
}

fn device_name(conn: &impl RequestConnection, deviceid: DeviceId) -> Option<String> {
    if let Ok(r) = conn.xinput_xiquery_device(deviceid) {
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

impl<T> HierarchyChangeEvent<T> for XIDeviceInfo {
    fn to_cmdline(&self, conn: &impl RequestConnection) -> Vec<String> {
        vec![
            self.deviceid.to_string(),
            format!("XI{:?}", DeviceType::try_from(self.type_).unwrap()),
            String::from_utf8_lossy(&self.name).to_string()
        ]
    }
}

impl<T> HierarchyChangeEvent<T> for HierarchyInfo {
    fn to_cmdline(&self, conn: &impl RequestConnection) -> Vec<String> {
        vec![
            self.deviceid.to_string(),
            if self.type_ == 0 {
                "".to_string()
            } else {
                format!("XI{:?}", DeviceType::try_from(self.type_).unwrap_or_else(|_| {
                    panic!("Unknown device type: {}", self.type_);
                }))
            },
            device_name(conn, self.deviceid).unwrap_or("".to_string())
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

    command.arg(format!("XI{:?}", change))
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

fn main() {
    let opt = Opt::from_args();
    println!("{:?}", opt);

    let (conn, _) = x11rb::connect(None).unwrap_or_else(|e| {
        eprintln!("Can't open X display: {}", e);
        exit(1);
    });

    let xinput_info = match conn.extension_information(xinput::X11_EXTENSION_NAME) {
        Ok(Some(info)) => info,
        Ok(None) | Err(_) => {
            eprintln!("X Input extension not available.");
            exit(1);
        }
    };

    println!("X Input extension opcode: {}", xinput_info.major_opcode);

    // We don’t want to inherit an open connection into the daemon
    drop(conn);

    if !opt.foreground {
        /*
        let daemonize = Daemonize::new()
            .stdout(std::io::stdout())
            .stderr(std::io::stderr())
        ;
        if opt.pidfile.is_some() {
            //daemonize.pid_file(opt.pidfile.unwrap().as_path());
        }

        daemonize.start().unwrap_or_else(|e| {
            println!("Cannot daemonize: {}", e);
            exit(1);
        });
        */
        if let Err(e) = daemon(false, opt.verbose) {
            eprintln!("Cannot daemonize: {}", e);
            exit(1);
        };

        println!("Daemonized.");
    }

    // Now that we’re in the daemon, reconnect to the X server
    let (conn, screen_num) = x11rb::connect(None).unwrap_or_else(|e| {
        eprintln!("Can't reconnect to the X display: {}", e);
        exit(1);
    });

    let screen = &conn.setup().roots[screen_num];

    if opt.bootstrap {
        if opt.verbose {
            println!("Bootstrapping events");
        }

        if let Ok(reply) = conn.xinput_xiquery_device(Device::All.into()) {
            let reply = reply.reply().unwrap_or_else(|e| {
                println!("Error: {}", e);
                exit(1);
            });
            for info in reply.infos {
                match DeviceType::try_from(info.type_).unwrap() {
                    DeviceType::MasterPointer |
                    DeviceType::MasterKeyboard => {
                        handle_device(&opt, &conn, &info, HierarchyMask::MasterAdded)
                    }
                    DeviceType::SlavePointer |
                    DeviceType::SlaveKeyboard => {
                        handle_device(&opt, &conn, &info, HierarchyMask::SlaveAdded);
                        handle_device(&opt, &conn, &info, HierarchyMask::DeviceEnabled)
                    }
                    _ => {}
                }
            }
        }
    }

    conn.xinput_xiselect_events(screen.root, &[EventMask {
        deviceid: Device::All.into(),
        mask: vec![XIEventMask::Hierarchy.into()]
    }]);

    conn.flush();
    loop {
        let event = match conn.wait_for_event() {
            Ok(event) => event,
            Err(e) => {
                eprintln!("Failed to get an event: {}", e);
                continue;
            }
        };
        if event.response_type() != GE_GENERIC_EVENT {
            continue;
        }
        if let Ok(hier_event) = HierarchyEvent::try_from(event) {
            if hier_event.extension != xinput_info.major_opcode {
                continue;
            }
            for info in hier_event.infos {
                let flags = IterableMask::from(info.flags)
                    .flat_map(HierarchyMask::try_from)
                    .collect::<Vec<HierarchyMask>>();
                for flag in flags {
                    handle_device(&opt, &conn, &info, flag);
                }
            }
        }
    }
}
