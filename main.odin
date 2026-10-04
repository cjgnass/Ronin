package main
import "core:fmt"
import rn "engine"

main :: proc() {
	engine: rn.Engine
	rn.init_engine(&engine)
	rn.run_engine(&engine)

}
