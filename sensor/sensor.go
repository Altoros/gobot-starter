package main

//#cgo LDFLAGS: -framework IOKit -framework CoreFoundation -framework ApplicationServices
//#include "smsget.h"
//#include "smc.h"
//#include "display.h"
import "C"

import (
	"encoding/json"
	"flag"
	"fmt"
	"log"
	"os"
	"time"

	"github.com/hybridgroup/gobot"
	"github.com/nobodyzzz/gobot/platforms/mqtt"
	"github.com/twinj/uuid"
)

type FanInfo struct {
	Min     float32 `json:"min"`
	Current float32 `json:"current"`
	Max     float32 `json:"max"`
}

type SMSInfo struct {
	X int `json:"x"`
	Y int `json:"y"`
	Z int `json:"z"`
}

type SensorInfo struct {
	D struct {
		Name     string    `json:"name"`
		CPUTemp  float64   `json:"cpu_temp"`
		FansInfo []FanInfo `json:"fans_info"`
		Position SMSInfo   `json:"position"`
	} `json:"d"`
}

type SetSpeedCmd struct {
	Fan   int `json:"fan"`
	Speed int `json:"speed"`
}

var org = flag.String("org", "quickstart", "your unique 6 character organization ID, assigned when you sign up with the service")
var apiToken = flag.String("api_token", "", "API token")
var deviceId = flag.String("id", uuid.Formatter(uuid.NewV4(), uuid.CleanHyphen), "Device ID")

func GatherSensorInfo() SensorInfo {
	res := SensorInfo{}
	pos := C.GetSmsData()
	cpuTemp := C.SMCGetTemperature()
	nFans := int(C.SMCGetTotalFansCount())

	res.D.Position = SMSInfo{int(pos.x), int(pos.y), int(pos.z)}
	res.D.CPUTemp = float64(cpuTemp)
	res.D.FansInfo = make([]FanInfo, 0)
	for i := 0; i < nFans; i++ {
		f := C.SMCGetFanInfo(C.int(i))
		res.D.FansInfo = append(res.D.FansInfo, FanInfo{float32(f.min), float32(f.current), float32(f.max)})
	}
	res.D.Name, _ = os.Hostname()
	return res
}

func main() {
	flag.Parse()
	gbot := gobot.NewGobot()
	username := ""

	fmt.Printf("Device id: %s\n", *deviceId)
	if *apiToken != "" {
		username = "use-token-auth"
	}
	mqttAdaptor := mqtt.NewMqttAdaptorWithAuth("sensor",
		fmt.Sprintf("tcp://%s.messaging.internetofthings.ibmcloud.com:1883", *org),
		fmt.Sprintf("d:%s:gobot-sensor:%s", *org, *deviceId), username, *apiToken)

	work := func() {
		gobot.Every(1*time.Second, func() {
			data, err := json.Marshal(GatherSensorInfo())
			if err == nil {
				mqttAdaptor.Publish("iot-2/evt/status/fmt/json", data)
			}
		})
		mqttAdaptor.On("iot-2/cmd/set_speed/fmt/json", func(data []byte) {
			var cmd SetSpeedCmd

			if err := json.Unmarshal(data, &cmd); err == nil {
				C.SMCSetFanSpeed(C.int(cmd.Fan), C.int(cmd.Speed))
			} else {
				log.Printf("Error: %s", err)
			}
		})
		mqttAdaptor.On("iot-2/cmd/inc_brightness/fmt/json", func(data []byte) {
			C.IncreaseBrightness()
		})
		mqttAdaptor.On("iot-2/cmd/dec_brightness/fmt/json", func(data []byte) {
			C.DecreaseBrightness()
		})
	}

	robot := gobot.NewRobot("mqttBot",
		[]gobot.Connection{mqttAdaptor},
		work,
	)
	gbot.AddRobot(robot)

	gbot.Start()
}

// https://github.com/max-horvath/htop-osx/blob/master/BatteryMeter.c
