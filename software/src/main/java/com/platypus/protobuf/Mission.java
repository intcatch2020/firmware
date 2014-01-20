// Code generated by Wire protocol buffer compiler, do not edit.
// Source file: ././platypus.proto
package com.platypus.protobuf;

import com.squareup.wire.Message;
import com.squareup.wire.ProtoField;

public final class Mission extends Message {

  @ProtoField(tag = 1)
  public final WaypointMission waypoint_mission;

  @ProtoField(tag = 2)
  public final RegionMission region_mission;

  private Mission(Builder builder) {
    super(builder);
    this.waypoint_mission = builder.waypoint_mission;
    this.region_mission = builder.region_mission;
  }

  @Override
  public boolean equals(Object other) {
    if (other == this) return true;
    if (!(other instanceof Mission)) return false;
    Mission o = (Mission) other;
    return equals(waypoint_mission, o.waypoint_mission)
        && equals(region_mission, o.region_mission);
  }

  @Override
  public int hashCode() {
    int result = hashCode;
    if (result == 0) {
      result = waypoint_mission != null ? waypoint_mission.hashCode() : 0;
      result = result * 37 + (region_mission != null ? region_mission.hashCode() : 0);
      hashCode = result;
    }
    return result;
  }

  public static final class Builder extends Message.Builder<Mission> {

    public WaypointMission waypoint_mission;
    public RegionMission region_mission;

    public Builder() {
    }

    public Builder(Mission message) {
      super(message);
      if (message == null) return;
      this.waypoint_mission = message.waypoint_mission;
      this.region_mission = message.region_mission;
    }

    public Builder waypoint_mission(WaypointMission waypoint_mission) {
      this.waypoint_mission = waypoint_mission;
      return this;
    }

    public Builder region_mission(RegionMission region_mission) {
      this.region_mission = region_mission;
      return this;
    }

    @Override
    public Mission build() {
      return new Mission(this);
    }
  }
}
