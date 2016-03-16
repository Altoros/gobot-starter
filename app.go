package main

import (
	"encoding/json"
	"fmt"
	"log"
	"strconv"
	"strings"

	"github.com/cloudfoundry-community/go-cfenv"
	"github.com/gin-gonic/gin"
	"github.com/gorilla/websocket"
	"github.com/hybridgroup/gobot"
	"github.com/nobodyzzz/gobot/platforms/mqtt"
	"github.com/twinj/uuid"
)

var wsupgrader = websocket.Upgrader{
	ReadBufferSize:  1024,
	WriteBufferSize: 1024,
}

func main() {
	var port, org, apiKey, apiToken string

	if appEnv, err := cfenv.Current(); err != nil {
		org = "quickstart"
		port = ":8080"
	} else {
		port = fmt.Sprintf(":%d", appEnv.Port)
		if iotfService, err := appEnv.Services.WithLabel("iotf-service"); err == nil {
			org = fmt.Sprintf("%s", iotfService[0].Credentials["org"])
			apiKey = fmt.Sprintf("%s", iotfService[0].Credentials["apiKey"])
			apiToken = fmt.Sprintf("%s", iotfService[0].Credentials["apiToken"].(string))
		}
	}

	r := gin.Default()
	r.LoadHTMLFiles("./static/index.html")

	log.Println("Setting handlers")

	r.GET("/", func(c *gin.Context) {
		c.HTML(200, "index.html", nil)
	})
	r.GET("/ws", func(c *gin.Context) {
		gbot := gobot.NewGobot()
		u := uuid.NewV4()
		deviceId := c.Query("device_id")

		if deviceId == "" {
			return
		}
		conn, err := wsupgrader.Upgrade(c.Writer, c.Request, nil)
		if err != nil {
			fmt.Println("Failed to set websocket upgrade: %+v", err)
			return
		}
		mqttAdaptor := mqtt.NewMqttAdaptorWithAuth("server",
			fmt.Sprintf("tcp://%s.messaging.internetofthings.ibmcloud.com:1883", org),
			fmt.Sprintf("a:%s:%s", org, uuid.Formatter(u, uuid.CleanHyphen)),
			apiKey,
			apiToken)

		work := func() {
			mqttAdaptor.On(fmt.Sprintf("iot-2/type/+/id/%s/evt/+/fmt/json", deviceId), func(data []byte) {
				if err := conn.WriteMessage(websocket.TextMessage, data); err != nil {
					gbot.Stop()
				}
			})
		}

		go func() {
			for {
				_, data, err := conn.ReadMessage()
				if err != nil {
					log.Printf("Error reading websocket %s", err)
					return
				}
				msg := string(data)
				if strings.HasPrefix(msg, "set_speed") {
					fields := strings.Split(msg, "/")
					fan, _ := strconv.Atoi(fields[1])
					speed, _ := strconv.Atoi(fields[2])
					cmd := gin.H{
						"fan":   fan,
						"speed": speed,
					}
					cmdData, err := json.Marshal(cmd)
					if err == nil {
						mqttAdaptor.Publish(fmt.Sprintf("iot-2/type/gobot-sensor/id/%s/cmd/set_speed/fmt/json", deviceId), cmdData)
					} else {
						log.Printf("Error sending command: %s", err)
					}
				} else if msg == "inc_brightness" || msg == "dec_brightness" {
					mqttAdaptor.Publish(fmt.Sprintf("iot-2/type/gobot-sensor/id/%s/cmd/%s/fmt/json", deviceId, msg), []byte{})
				}
			}
		}()

		robot := gobot.NewRobot("mqttBot",
			[]gobot.Connection{mqttAdaptor},
			work,
		)
		gbot.AutoStop = false

		gbot.AddRobot(robot)

		gbot.Start()

	})
	r.Static("/static", "./static")
	r.Run(port)

}

// https://docs.internetofthings.ibmcloud.com/ja/applications/mqtt.html
// https://quickstart.internetofthings.ibmcloud.com/iotsensor/
