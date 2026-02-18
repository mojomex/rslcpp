FROM ros:humble

SHELL ["/bin/bash", "-o", "pipefail", "-c"]

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    python3-colcon-common-extensions \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /rslcpp
COPY . .

RUN rm -rf build install log && \
    source /opt/ros/humble/setup.bash && \
    colcon build --cmake-args -DBUILD_TESTING=OFF -DCMAKE_BUILD_TYPE=Release && \
    source install/setup.bash && \
    ros2 run rslcpp_test determinism --ros-args -p use_sim_time:=true && \
    rm -f callback_execution_order.txt
